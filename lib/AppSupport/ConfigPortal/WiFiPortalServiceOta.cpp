#include "ConfigPortal/WiFiPortalService.h"
#include "FileSystem/BridgeFileSystem.h"

#include <Arduino.h>
#include <algorithm>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>

namespace
{
    constexpr const char *kRemoteOtaBaseUrl = "https://sunbox.github.io/radpro-wifi-bridge/web-install/";
    constexpr const char *kRemoteManifestUrl = "https://sunbox.github.io/radpro-wifi-bridge/web-install/manifest.json";
    constexpr unsigned long kRemoteManifestRefreshMs = 5UL * 60UL * 1000UL;
    constexpr size_t kOtaDownloadBuffer = 1024;
}

void WiFiPortalService::sendJson(int code, const String &body)
{
    if (!manager_.server)
        return;
    manager_.server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    manager_.server->sendHeader("Pragma", "no-cache");
    manager_.server->sendHeader("Expires", "0");
    manager_.server->send(code, "application/json", body);
}

void WiFiPortalService::sendOtaPage(const String &message)
{
    if (!manager_.server)
        return;

    bool isError = message.startsWith(F("ERROR:"));
    String display = isError ? message.substring(6) : message;
    display.trim();

    String noticeClass;
    if (!display.length())
    {
        noticeClass = F("hidden");
    }
    else
    {
        noticeClass = isError ? F("error") : F("success");
    }

    TemplateReplacements vars = {
        {"{{NOTICE_CLASS}}", noticeClass},
        {"{{NOTICE_TEXT}}", htmlEscape(display)}};

    appendCommonTemplateVars(vars);
    sendTemplate("/portal/ota.html", vars);
}

void WiFiPortalService::resetOtaProgress()
{
    portENTER_CRITICAL(&otaLock_);
    otaProgressMessage_.clear();
    otaBytesExpected_ = 0;
    otaBytesWritten_ = 0;
    otaLastProgressMs_ = millis();
    portEXIT_CRITICAL(&otaLock_);
}

void WiFiPortalService::setOtaProgress(const String &message, size_t totalBytes, size_t writtenBytes)
{
    portENTER_CRITICAL(&otaLock_);
    otaProgressMessage_ = message;
    otaBytesExpected_ = totalBytes;
    otaBytesWritten_ = writtenBytes;
    otaLastProgressMs_ = millis();
    portEXIT_CRITICAL(&otaLock_);
}

void WiFiPortalService::updateOtaBytes(size_t writtenBytes)
{
    portENTER_CRITICAL(&otaLock_);
    otaBytesWritten_ = writtenBytes;
    otaLastProgressMs_ = millis();
    portEXIT_CRITICAL(&otaLock_);
}

void WiFiPortalService::setOtaMessage(const String &message)
{
    portENTER_CRITICAL(&otaLock_);
    otaProgressMessage_ = message;
    otaLastProgressMs_ = millis();
    portEXIT_CRITICAL(&otaLock_);
}

bool WiFiPortalService::decodeBase64Chunk(const String &input, std::vector<uint8_t> &out, String &error)
{
    if (!input.length())
    {
        error = F("No data received.");
        return false;
    }

    size_t estimated = ((input.length() + 3) / 4) * 3;
    out.resize(estimated);
    size_t decodedLen = 0;
    int rc = mbedtls_base64_decode(out.data(),
                                   out.size(),
                                   &decodedLen,
                                   reinterpret_cast<const unsigned char *>(input.c_str()),
                                   input.length());
    if (rc != 0)
    {
        error = String(F("Base64 decode failed: ")) + rc;
        return false;
    }
    out.resize(decodedLen);
    return true;
}

void WiFiPortalService::refreshLatestRemoteVersion(bool force)
{
    unsigned long now = millis();
    if (!force && latestRemoteCheckMs_ != 0 && now - latestRemoteCheckMs_ < kRemoteManifestRefreshMs)
        return;

    if (manifestTaskHandle_)
        return;

    manifestForceRefresh_ = force;
    BaseType_t created = xTaskCreatePinnedToCore(&WiFiPortalService::manifestTaskThunk,
                                                 "otaManifest",
                                                 8192,
                                                 this,
                                                 1,
                                                 &manifestTaskHandle_,
                                                 1);
    if (created != pdPASS)
    {
        manifestTaskHandle_ = nullptr;
        latestRemoteError_ = F("Manifest task start failed.");
        latestRemoteCheckMs_ = now;
        manifestForceRefresh_ = false;
    }
}

void WiFiPortalService::handleOtaStatus()
{
    if (!manager_.server)
        return;

    refreshLatestRemoteVersion(false);

    OtaUpdateService::Status state = otaService_.status();
    JsonDocument doc;
    doc["currentVersion"] = BRIDGE_FIRMWARE_VERSION;
    if (latestRemoteVersion_.length())
        doc["latestVersion"] = latestRemoteVersion_;
    if (latestRemoteError_.length())
        doc["latestError"] = latestRemoteError_;
    doc["latestCheckedMs"] = latestRemoteCheckMs_;

    JsonObject ota = doc["ota"].to<JsonObject>();
    ota["busy"] = state.busy;
    ota["needsReboot"] = state.needsReboot;
    ota["lastError"] = state.lastError;
    ota["partsCompleted"] = static_cast<uint32_t>(state.partsCompleted);
    ota["partsTotal"] = static_cast<uint32_t>(state.partsTotal);
    ota["targetVersion"] = state.targetVersion;
    ota["taskActive"] = otaTaskHandle_ != nullptr;

    String messageCopy;
    size_t bytesWritten = 0;
    size_t bytesTotal = 0;
    portENTER_CRITICAL(&otaLock_);
    messageCopy = otaProgressMessage_;
    bytesWritten = otaBytesWritten_;
    bytesTotal = otaBytesExpected_;
    portEXIT_CRITICAL(&otaLock_);

    ota["message"] = messageCopy;
    ota["bytesWritten"] = static_cast<uint32_t>(bytesWritten);
    ota["bytesTotal"] = static_cast<uint32_t>(bytesTotal);

    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

void WiFiPortalService::handleOtaFetch()
{
    if (!manager_.server)
        return;

    if (otaTaskHandle_)
    {
        JsonDocument doc;
        doc["error"] = F("OTA download already running.");
        String body;
        serializeJson(doc, body);
        sendJson(409, body);
        return;
    }

    OtaUpdateService::Status status = otaService_.status();
    if (status.busy)
    {
        JsonDocument doc;
        doc["error"] = F("OTA process already active.");
        String body;
        serializeJson(doc, body);
        sendJson(409, body);
        return;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        JsonDocument doc;
        doc["error"] = F("Wi-Fi is not connected.");
        String body;
        serializeJson(doc, body);
        sendJson(503, body);
        return;
    }

    resetOtaProgress();
    setOtaProgress(F("Preparing remote download…"), 0, 0);

    BaseType_t created = xTaskCreatePinnedToCore(&WiFiPortalService::otaTaskThunk,
                                                 "otaRemote",
                                                 8192,
                                                 this,
                                                 1,
                                                 &otaTaskHandle_,
                                                 1);
    if (created != pdPASS)
    {
        otaTaskHandle_ = nullptr;
        JsonDocument doc;
        doc["error"] = F("Unable to start OTA task.");
        String body;
        serializeJson(doc, body);
        sendJson(500, body);
        return;
    }

    JsonDocument doc;
    doc["started"] = true;
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

void WiFiPortalService::handleOtaUploadBegin()
{
    if (!manager_.server)
        return;

    if (otaTaskHandle_)
    {
        JsonDocument doc;
        doc["error"] = F("Remote OTA already running.");
        String body;
        serializeJson(doc, body);
        sendJson(409, body);
        return;
    }

    String manifest = manager_.server->arg("plain");
    manifest.trim();
    if (!manifest.length())
    {
        JsonDocument doc;
        doc["error"] = F("Manifest payload missing.");
        String body;
        serializeJson(doc, body);
        sendJson(400, body);
        return;
    }

    if (!otaService_.begin(manifest))
    {
        OtaUpdateService::Status state = otaService_.status();
        String err = state.lastError.length() ? state.lastError : String(F("Manifest rejected."));
        JsonDocument doc;
        doc["error"] = err;
        String body;
        serializeJson(doc, body);
        sendJson(400, body);
        return;
    }

    resetOtaProgress();
    setOtaProgress(F("Manifest uploaded; awaiting binaries…"), 0, 0);

    JsonDocument doc;
    doc["ok"] = true;
    doc["mode"] = "upload";
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

void WiFiPortalService::handleOtaUploadPartBegin()
{
    if (!manager_.server)
        return;

    if (!otaService_.status().busy)
    {
        JsonDocument doc;
        doc["error"] = F("OTA session is not active.");
        String body;
        serializeJson(doc, body);
        sendJson(409, body);
        return;
    }

    String path = manager_.server->arg("path");
    String offsetArg = manager_.server->arg("offset");
    String sizeArg = manager_.server->arg("size");
    path.trim();
    offsetArg.trim();
    sizeArg.trim();
    if (!path.length() || !offsetArg.length() || !sizeArg.length())
    {
        JsonDocument doc;
        doc["error"] = F("Missing part metadata.");
        String body;
        serializeJson(doc, body);
        sendJson(400, body);
        return;
    }

    uint32_t offset = static_cast<uint32_t>(strtoul(offsetArg.c_str(), nullptr, 10));
    size_t size = static_cast<size_t>(strtoull(sizeArg.c_str(), nullptr, 10));

    if (!otaService_.beginPart(path, offset, size))
    {
        OtaUpdateService::Status state = otaService_.status();
        String err = state.lastError.length() ? state.lastError : String(F("beginPart() failed."));
        JsonDocument doc;
        doc["error"] = err;
        String body;
        serializeJson(doc, body);
        sendJson(400, body);
        return;
    }

    setOtaProgress(String(F("Uploading ")) + path, size, 0);

    JsonDocument doc;
    doc["ok"] = true;
    doc["path"] = path;
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

void WiFiPortalService::handleOtaUploadPartChunk()
{
    if (!manager_.server)
        return;

    if (!otaService_.status().busy)
    {
        JsonDocument doc;
        doc["error"] = F("OTA session is not active.");
        String body;
        serializeJson(doc, body);
        sendJson(409, body);
        return;
    }

    String encoded = manager_.server->arg("plain");
    encoded.trim();
    String error;
    if (!decodeBase64Chunk(encoded, otaChunkBuffer_, error))
    {
        JsonDocument doc;
        doc["error"] = error;
        String body;
        serializeJson(doc, body);
        sendJson(400, body);
        return;
    }

    if (!otaService_.writePartChunk(otaChunkBuffer_.data(), otaChunkBuffer_.size()))
    {
        OtaUpdateService::Status state = otaService_.status();
        String err = state.lastError.length() ? state.lastError : String(F("Chunk write failed."));
        JsonDocument doc;
        doc["error"] = err;
        String body;
        serializeJson(doc, body);
        sendJson(500, body);
        return;
    }

    portENTER_CRITICAL(&otaLock_);
    otaBytesWritten_ += otaChunkBuffer_.size();
    otaLastProgressMs_ = millis();
    portEXIT_CRITICAL(&otaLock_);

    JsonDocument doc;
    doc["ok"] = true;
    doc["bytes"] = static_cast<uint32_t>(otaChunkBuffer_.size());
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

void WiFiPortalService::handleOtaUploadPartFinish()
{
    if (!manager_.server)
        return;

    if (!otaService_.status().busy)
    {
        JsonDocument doc;
        doc["error"] = F("OTA session is not active.");
        String body;
        serializeJson(doc, body);
        sendJson(409, body);
        return;
    }

    if (!otaService_.finalizePart())
    {
        OtaUpdateService::Status state = otaService_.status();
        String err = state.lastError.length() ? state.lastError : String(F("Part finalize failed."));
        JsonDocument doc;
        doc["error"] = err;
        String body;
        serializeJson(doc, body);
        sendJson(500, body);
        return;
    }

    String path = manager_.server->arg("path");
    if (path.length())
        setOtaMessage(String(F("Finished ")) + path);

    JsonDocument doc;
    doc["ok"] = true;
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

void WiFiPortalService::handleOtaUploadFinish()
{
    if (!manager_.server)
        return;

    if (!otaService_.finish())
    {
        OtaUpdateService::Status state = otaService_.status();
        String err = state.lastError.length() ? state.lastError : String(F("OTA completion failed."));
        JsonDocument doc;
        doc["error"] = err;
        String body;
        serializeJson(doc, body);
        sendJson(500, body);
        return;
    }

    BridgeFileSystem::mount(log_, "ota-upload-finish", false);
    setOtaMessage(F("Upload complete; applying update…"));
    scheduleRestart("Uploaded OTA");

    JsonDocument doc;
    doc["ok"] = true;
    doc["reboot"] = true;
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

void WiFiPortalService::handleOtaCancel()
{
    if (!manager_.server)
        return;

    otaService_.abort(F("Cancelled by user."));
    BridgeFileSystem::mount(log_, "ota-cancel", false);
    resetOtaProgress();

    JsonDocument doc;
    doc["ok"] = true;
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

bool WiFiPortalService::parseManifestParts(const String &manifestJson, std::vector<ManifestPart> &parts, String &version, String &error)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, manifestJson);
    if (err)
    {
        error = String(F("Manifest parse failed: ")) + err.c_str();
        return false;
    }

    version = doc["version"].as<const char *>();
    JsonArray builds = doc["builds"].as<JsonArray>();
    if (!builds || builds.size() == 0)
    {
        error = F("Manifest missing builds array.");
        return false;
    }

    JsonArray manifestParts = builds[0]["parts"].as<JsonArray>();
    if (!manifestParts || manifestParts.size() == 0)
    {
        error = F("Manifest missing parts.");
        return false;
    }

    parts.clear();
    parts.reserve(manifestParts.size());
    for (const JsonVariantConst &entry : manifestParts)
    {
        ManifestPart part;
        part.path = entry["path"].as<const char *>();
        part.offset = entry["offset"] | 0;
        if (!part.path.length())
        {
            error = F("Manifest entry missing path.");
            parts.clear();
            return false;
        }
        parts.push_back(part);
    }
    return true;
}

bool WiFiPortalService::fetchRemoteManifest(String &manifestJson, String &version, String &error)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        error = F("Wi-Fi disconnected.");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);
    if (!http.begin(client, kRemoteManifestUrl))
    {
        error = F("Manifest request failed.");
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        error = String(F("Manifest HTTP error: ")) + code;
        http.end();
        return false;
    }

    manifestJson = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, manifestJson);
    if (err)
    {
        error = String(F("Manifest decode failed: ")) + err.c_str();
        return false;
    }

    version = doc["version"].as<const char *>();
    if (!version.length())
        version = F("unknown");
    return true;
}

bool WiFiPortalService::downloadRemotePart(const String &baseUrl, const String &path, uint32_t offset, String &error)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        error = F("Wi-Fi disconnected.");
        return false;
    }

    String url = baseUrl;
    if (!url.endsWith("/"))
        url += '/';
    url += path;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(20000);
    if (!http.begin(client, url))
    {
        error = F("Part request failed.");
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        error = String(F("Part HTTP error: ")) + code;
        http.end();
        return false;
    }

    int contentLen = http.getSize();
    if (contentLen <= 0)
    {
        error = F("Missing Content-Length.");
        http.end();
        return false;
    }

    size_t expected = static_cast<size_t>(contentLen);
    if (!otaService_.beginPart(path, offset, expected))
    {
        OtaUpdateService::Status state = otaService_.status();
        error = state.lastError.length() ? state.lastError : String(F("beginPart() failed."));
        http.end();
        return false;
    }

    setOtaProgress(String(F("Flashing ")) + path, expected, 0);

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[kOtaDownloadBuffer];
    size_t written = 0;
    while (http.connected() && written < expected)
    {
        if (!stream->available())
        {
            delay(1);
            continue;
        }

        size_t toRead = std::min(static_cast<size_t>(stream->available()), sizeof(buffer));
        int read = stream->readBytes(buffer, toRead);
        if (read <= 0)
        {
            error = F("Stream read failed.");
            http.end();
            return false;
        }

        if (!otaService_.writePartChunk(buffer, read))
        {
            OtaUpdateService::Status state = otaService_.status();
            error = state.lastError.length() ? state.lastError : String(F("Flash write failed."));
            http.end();
            return false;
        }

        written += static_cast<size_t>(read);
        updateOtaBytes(written);
        yield();
    }

    http.end();

    if (!otaService_.finalizePart())
    {
        OtaUpdateService::Status state = otaService_.status();
        error = state.lastError.length() ? state.lastError : String(F("finalizePart() failed."));
        return false;
    }

    return true;
}

void WiFiPortalService::otaTaskThunk(void *param)
{
    WiFiPortalService *service = static_cast<WiFiPortalService *>(param);
    service->runRemoteFetchTask();
    service->otaTaskHandle_ = nullptr;
    vTaskDelete(nullptr);
}

void WiFiPortalService::manifestTaskThunk(void *param)
{
    WiFiPortalService *service = static_cast<WiFiPortalService *>(param);
    service->runManifestFetchTask(service->manifestForceRefresh_);
    service->manifestTaskHandle_ = nullptr;
    service->manifestForceRefresh_ = false;
    vTaskDelete(nullptr);
}

void WiFiPortalService::runRemoteFetchTask()
{
    resetOtaProgress();
    setOtaMessage(F("Downloading manifest…"));

    String manifest;
    String version;
    String error;
    if (!fetchRemoteManifest(manifest, version, error))
    {
        setOtaMessage(String(F("Manifest failed: ")) + error);
        otaService_.abort(error);
        BridgeFileSystem::mount(log_, "ota-remote-failed", false);
        return;
    }

    latestRemoteVersion_ = version;
    latestRemoteError_.clear();
    latestRemoteCheckMs_ = millis();

    std::vector<ManifestPart> parts;
    String manifestVersion;
    if (!parseManifestParts(manifest, parts, manifestVersion, error))
    {
        setOtaMessage(String(F("Manifest invalid: ")) + error);
        otaService_.abort(error);
        BridgeFileSystem::mount(log_, "ota-remote-failed", false);
        return;
    }

    if (!otaService_.begin(manifest))
    {
        OtaUpdateService::Status state = otaService_.status();
        String err = state.lastError.length() ? state.lastError : String(F("Unable to start OTA."));
        setOtaMessage(String(F("OTA begin failed: ")) + err);
        otaService_.abort(err);
        BridgeFileSystem::mount(log_, "ota-remote-failed", false);
        return;
    }

    for (const auto &part : parts)
    {
        setOtaMessage(String(F("Downloading ")) + part.path);
        if (!downloadRemotePart(kRemoteOtaBaseUrl, part.path, part.offset, error))
        {
            setOtaMessage(String(F("Download failed: ")) + error);
            otaService_.abort(error);
            BridgeFileSystem::mount(log_, "ota-remote-failed", false);
            return;
        }
    }

    if (!otaService_.finish())
    {
        OtaUpdateService::Status state = otaService_.status();
        String err = state.lastError.length() ? state.lastError : String(F("finish() failed."));
        setOtaMessage(String(F("OTA finalize failed: ")) + err);
        otaService_.abort(err);
        BridgeFileSystem::mount(log_, "ota-remote-failed", false);
        return;
    }

    BridgeFileSystem::mount(log_, "ota-remote-complete", false);
    setOtaMessage(F("Update complete. Rebooting…"));
    scheduleRestart("Remote OTA");
}

void WiFiPortalService::runManifestFetchTask(bool force)
{
    (void)force;
    String manifest;
    String version;
    String error;
    if (!fetchRemoteManifest(manifest, version, error))
    {
        latestRemoteError_ = error;
        latestRemoteCheckMs_ = millis();
        return;
    }

    latestRemoteVersion_ = version;
    latestRemoteError_.clear();
    latestRemoteCheckMs_ = millis();
}
