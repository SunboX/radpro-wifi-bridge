// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later
#include "RadProUpdateService.h"
#include "Stm32Bootloader.h"
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include <algorithm>

namespace
{
constexpr const char *kStagingFile = "/radpro-update.bin";
constexpr uint32_t kFlashBase = 0x08000000;
constexpr size_t kPageSize = 0x800;
class UsbTransport : public RadProUpdate::Transport
{
public:
    UsbTransport(UsbCdcHost &usb, uint32_t generation) : usb_(usb), generation_(generation) {}
    bool write(const uint8_t *data, size_t size) override
    { return usb_.connectionGeneration() == generation_ && usb_.firmwareWrite(data, size); }
    bool read(uint8_t *data, size_t size, uint32_t timeout) override
    { return usb_.connectionGeneration() == generation_ && usb_.firmwareRead(data, size, timeout) && usb_.connectionGeneration() == generation_; }
    void drain() override { usb_.firmwareDrain(); }
private:
    UsbCdcHost &usb_;
    uint32_t generation_;
};
}

RadProUpdateService::RadProUpdateService(UsbCdcHost &usb, DeviceManager &manager, DeviceInfoStore &info)
    : usb_(usb), manager_(manager), info_(info) {}

bool RadProUpdateService::sameConnection() const
{
    return usb_.isConnected() && usb_.connectionGeneration() == generation_;
}

bool RadProUpdateService::beginUpload(const String &locale, String &error)
{
    if (ownsUsb()) { error = "An update or bootloader recovery is active."; return false; }
    const auto snap = info_.snapshot();
    const auto *model = RadProUpdate::findModel(snap.model.c_str());
    const auto *image = model ? RadProUpdate::findImage(*model, locale.c_str()) : nullptr;
    if (!usb_.isConnected() || !snap.deviceId.length() || !model || !image ||
        !RadProUpdate::supportsFirmware(snap.firmware.c_str()))
    { error = "Requires a supported connected detector running stable Rad Pro 3.0.2 or later (3.x)."; return false; }
    // Never include the final settings page in an update.
    if (image->size > model->flashSize - kPageSize || image->size % 4)
    { error = "Firmware image exceeds the application area."; return false; }
    if (upload_) upload_.close();
    if (LittleFS.totalBytes() - LittleFS.usedBytes() < image->size + 8192)
    { error = "Insufficient staging space. Cancel the previous upload first."; return false; }
    upload_ = LittleFS.open(kStagingFile, "w");
    if (!upload_) { error = "Could not create firmware staging file."; return false; }
    model_ = model;
    image_ = image;
    generation_ = usb_.connectionGeneration();
    deviceId_ = snap.deviceId;
    uploaded_ = 0;
    progress_ = 0;
    lastUploadMs_ = millis();
    error_ = "";
    phase_ = "uploading";
    return true;
}

bool RadProUpdateService::uploadChunk(size_t offset, const uint8_t *data, size_t size, String &error)
{
    if (busy_ || !upload_ || !image_ || !sameConnection() || offset != uploaded_ ||
        !size || size > 3072 || size > image_->size - uploaded_)
    { error = "Invalid chunk, offset, or changed USB connection."; return false; }
    if (upload_.write(data, size) != size) { error = "Staging write failed."; return false; }
    uploaded_ += size;
    lastUploadMs_ = millis();
    if (uploaded_ == image_->size)
    {
        upload_.close();
        phase_ = "staged";
    }
    return true;
}

bool RadProUpdateService::validateImage(String &error)
{
    File file = LittleFS.open(kStagingFile, "r");
    if (!file || file.size() != image_->size) { error = "Firmware upload is incomplete."; return false; }
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    mbedtls_sha256_starts_ret(&context, 0);
    uint8_t buffer[1024], digest[32];
    size_t count = 0;
    while (count < image_->size)
    {
        const size_t n = file.read(buffer, std::min(sizeof(buffer), image_->size - count));
        if (!n) break;
        mbedtls_sha256_update_ret(&context, buffer, n);
        count += n;
    }
    mbedtls_sha256_finish_ret(&context, digest);
    mbedtls_sha256_free(&context);
    char hex[65];
    for (size_t i = 0; i < 32; ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    if (count != image_->size || strcmp(hex, image_->sha256) != 0)
    { error = "SHA-256 mismatch: use the official 3.1.1 image for this model and language."; return false; }
    return true;
}

bool RadProUpdateService::start(String &error)
{
    if (busy_ || !image_ || !sameConnection() || uploaded_ != image_->size)
    { error = "No complete upload for the current USB connection."; return false; }
    if (!validateImage(error)) return false;
    // Called on the application task, before launching the worker.
    if (!recoveryRequired_) resumeManager_ = manager_.enabled();
    manager_.stop();
    busy_ = true;
    done_ = false;
    progress_ = 0;
    error_ = "";
    phase_ = "connecting";
    if (xTaskCreate(task, "radpro_update", 8192, this, 3, nullptr) != pdPASS)
    {
        busy_ = false;
        phase_ = "failed";
        if (!recoveryRequired_ && resumeManager_) manager_.start();
        error = "Could not start update task.";
        error_ = error;
        return false;
    }
    return true;
}

bool RadProUpdateService::cancel(String &error)
{
    if (ownsUsb()) { error = "Flashing cannot be cancelled. Retry the staged image or use the upstream installer for recovery."; return false; }
    if (upload_) upload_.close();
    LittleFS.remove(kStagingFile);
    image_ = nullptr;
    uploaded_ = 0;
    phase_ = "idle";
    error_ = "";
    return true;
}

void RadProUpdateService::poll()
{
    if (busy_ && done_)
    {
        busy_ = false;
        done_ = false;
        if (!recoveryRequired_)
        {
            info_.clearLiveData();
            if (resumeManager_) manager_.start();
        }
    }
    if (!ownsUsb() && upload_ && millis() - lastUploadMs_ > 120000)
    {
        String error;
        cancel(error);
        error_ = "Upload expired; select the firmware again.";
    }
}

void RadProUpdateService::task(void *arg)
{
    static_cast<RadProUpdateService *>(arg)->run();
    vTaskDelete(nullptr);
}

void RadProUpdateService::run()
{
    const bool success = flash(error_);
    usb_.endFirmwareSession();
    if (success)
    {
        recoveryRequired_ = false;
        LittleFS.remove(kStagingFile);
        phase_ = "complete";
    }
    else phase_ = "failed";
    done_ = true;
}

bool RadProUpdateService::readLine(String &line, uint32_t timeout)
{
    line = "";
    const unsigned long started = millis();
    while (millis() - started < timeout && line.length() < 256 && sameConnection())
    {
        uint8_t c;
        if (!usb_.firmwareRead(&c, 1, 100)) continue;
        if (c == '\n' && line.length()) { line.trim(); return true; }
        if (c != '\r') line += static_cast<char>(c);
    }
    return false;
}

bool RadProUpdateService::queryIdentity(String &error)
{
    usb_.firmwareDrain();
    const char *query = "GET deviceId\r\n";
    String line;
    if (!usb_.firmwareWrite(reinterpret_cast<const uint8_t *>(query), strlen(query)) ||
        !readLine(line, 4000))
    { error = "Detector identity request failed."; return false; }
    const String prefix = String("OK ") + model_->name + ";";
    const String suffix = String(";") + deviceId_;
    if (!line.startsWith(prefix) || !line.endsWith(suffix))
    { error = "Detector identity changed; update stopped."; return false; }
    return true;
}

bool RadProUpdateService::flash(String &error)
{
    if (!sameConnection() || !usb_.beginFirmwareSession())
    { error = "Could not reserve the connected USB detector."; return false; }
    if (!recoveryRequired_)
    {
        if (!queryIdentity(error)) return false;
        const char *command = "START bootloader\r\n";
        String line;
        // Once sent, a lost ACK may still mean the detector entered its bootloader.
        recoveryRequired_ = true;
        if (!usb_.firmwareWrite(reinterpret_cast<const uint8_t *>(command), strlen(command)) ||
            !readLine(line, 3000) || line != "OK")
        { error = "Bootloader entry was not acknowledged. Retry or use the upstream installer."; return false; }
        delay(500);
    }
    if (!usb_.setFirmwareLineCoding(57600, true))
    { error = "USB adapter cannot configure 57600 baud, 8E1."; return false; }
    const bool gq = !model_->extendedErase;
    if (gq)
    {
        if (!usb_.setFirmwareSignals(false, false)) { error = "Could not select GQ bootloader."; return false; }
        delay(50);
        if (!usb_.setFirmwareSignals(false, true)) { error = "Could not reset GQ detector."; return false; }
        delay(500);
    }
    UsbTransport transport(usb_, generation_);
    RadProUpdate::Stm32Bootloader bootloader(transport);
    if (!bootloader.sync() || !bootloader.identify(model_->productId))
    { error = "Bootloader did not respond with the expected chip ID. No erase was issued."; return false; }
    File file = LittleFS.open(kStagingFile, "r");
    if (!file) { error = "Firmware staging file is unavailable."; return false; }
    // Probe read access before erasing. Never unprotect or mass erase a detector.
    uint8_t buffer[256], verify[256];
    if (!bootloader.readBlock(kFlashBase, verify, sizeof(verify)))
    { error = "Flash read access failed; read-protected devices require the upstream installer."; return false; }
    phase_ = "erasing";
    const size_t pages = (image_->size + kPageSize - 1) / kPageSize;
    for (size_t page = 0; page < pages; ++page)
    {
        if (!bootloader.erasePage(page, model_->extendedErase))
        { error = "Page erase failed. Keep power connected and retry the staged image."; return false; }
        progress_ = (page + 1) * 20 / pages;
        delay(1);
    }
    phase_ = "writing";
    // Vector table last: never start a partially written application.
    size_t written = 0;
    for (size_t end = (image_->size + 255) / 256 * 256; end; end -= 256)
    {
        const size_t offset = end - 256;
        const size_t size = std::min<size_t>(256, image_->size - offset);
        if (!file.seek(offset) || file.read(buffer, size) != size ||
            !bootloader.writeBlock(kFlashBase + offset, buffer, size) ||
            !bootloader.readBlock(kFlashBase + offset, verify, size) || memcmp(buffer, verify, size) != 0)
        { error = "Flash write/readback verification failed. Keep power connected and retry the staged image."; return false; }
        written += size;
        progress_ = 20 + written * 75 / image_->size;
        delay(1);
    }
    phase_ = "restarting";
    if (gq)
    {
        if (!usb_.setFirmwareSignals(true, false)) { error = "GQ application reset failed."; return false; }
        delay(50);
        if (!usb_.setFirmwareSignals(true, true)) { error = "GQ application reset failed."; return false; }
    }
    else if (!bootloader.go(kFlashBase)) { error = "Application start failed."; return false; }
    if (!usb_.setFirmwareLineCoding(115200, false)) { error = "Could not restore serial settings."; return false; }
    delay(2000);
    if (!queryIdentity(error)) return false;
    // Also confirm the installed version/locale; matching identity alone is insufficient.
    const char *query = "GET deviceId\r\n";
    String line;
    usb_.firmwareDrain();
    if (!usb_.firmwareWrite(reinterpret_cast<const uint8_t *>(query), strlen(query)) ||
        !readLine(line, 4000) || line.indexOf(String(";Rad Pro 3.1.1/") + image_->locale + ";") < 0)
    { error = "Installed firmware version could not be confirmed."; return false; }
    progress_ = 100;
    return true;
}

String RadProUpdateService::statusJson() const
{
    JsonDocument doc;
    const auto snap = info_.snapshot();
    const auto *model = RadProUpdate::findModel(snap.model.c_str());
    doc["model"] = snap.model;
    doc["firmware"] = snap.firmware;
    doc["version"] = "3.1.1";
    doc["supported"] = usb_.isConnected() && snap.deviceId.length() && model && RadProUpdate::supportsFirmware(snap.firmware.c_str());
    doc["busy"] = busy();
    doc["recoveryRequired"] = recoveryRequired_.load();
    doc["phase"] = phase_.load();
    doc["progress"] = progress_.load();
    doc["uploaded"] = uploaded_;
    doc["expected"] = image_ ? image_->size : 0;
    if (!busy_ || done_) doc["error"] = error_;
    if (model)
    {
        JsonArray locales = doc["locales"].to<JsonArray>();
        for (const auto &image : RadProUpdate::images)
        {
            if (strcmp(image.environment, model->environment)) continue;
            JsonObject entry = locales.add<JsonObject>();
            entry["locale"] = image.locale;
            entry["size"] = image.size;
            entry["filename"] = String("radpro-") + image.environment + "-" + image.locale + "-3.1.1.bin";
            entry["url"] = String("https://gissio.github.io/radpro-installer/") + image.environment + "/firmware/" + entry["filename"].as<String>();
        }
    }
    String json;
    serializeJson(doc, json);
    return json;
}
