#include "OpenSenseMap/OpenSenseMapPublisher.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "ConfigPortal/WiFiPortalService.h"
#include <WebServer.h>
#include "Led/LedController.h"

namespace
{
    constexpr const char *kOpenSenseMapRootCa =
        "-----BEGIN CERTIFICATE-----\n"
        "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\n"
        "-----END CERTIFICATE-----\n";
}

namespace
{
    constexpr const char *kHost = "api.opensensemap.org";
    constexpr uint16_t kPort = 443;
    constexpr unsigned long kMinPublishGapMs = 4000;
    constexpr unsigned long kRetryBackoffMs = 10000;
    constexpr unsigned long kMaxRetryBackoffMs = 60000;

    int logTlsError(Print &log, WiFiClientSecure &client, const char *context, String *errText = nullptr)
    {
        char err[128] = {0};
        int code = client.lastError(err, sizeof(err));
        if (errText)
            *errText = err;
        if (code == 0)
            return 0;

        log.print(F("OpenSenseMap: "));
        log.print(context);
        log.print(F(" TLS error "));
        log.print(code);
        if (err[0])
        {
            log.print(F(" ("));
            log.print(err);
            log.print(F(")"));
        }
        log.println();
        return code;
    }
}

OpenSenseMapPublisher::OpenSenseMapPublisher(AppConfig &config, Print &log, const char *bridgeVersion)
    : config_(config),
      log_(log),
      bridgeVersion_(bridgeVersion ? bridgeVersion : "")
{
}

void OpenSenseMapPublisher::begin()
{
    updateConfig();
}

void OpenSenseMapPublisher::updateConfig()
{
    // No-op for now; pending values are populated via onCommandResult.
    // Leaving this hook in case we need to react to config changes later.
}

void OpenSenseMapPublisher::loop()
{
    if (paused_)
        return;
    publishPending();
}

void OpenSenseMapPublisher::onCommandResult(DeviceManager::CommandType type, const String &value)
{
    if (paused_)
        return;
    if (!value.length())
        return;

    switch (type)
    {
    case DeviceManager::CommandType::TubeRate:
        pendingTubeValue_ = value;
        haveTubeValue_ = true;
        // Tube values on their own are not published until we also have a dose reading.
        break;
    case DeviceManager::CommandType::TubeDoseRate:
        pendingDoseValue_ = value;
        haveDoseValue_ = true;
        if (haveTubeValue_)
        {
            pendingPublish_ = true;
            suppressUntilMs_ = 0;
        }
        break;
    default:
        break;
    }
}

bool OpenSenseMapPublisher::isEnabled() const
{
    if (!config_.openSenseMapEnabled)
        return false;
    if (!config_.openSenseBoxId.length())
        return false;
    if (!config_.openSenseApiKey.length())
        return false;
    return true;
}

bool OpenSenseMapPublisher::publishPending()
{
    if (paused_)
        return true;
    if (!pendingPublish_)
        return false;

    if (!isEnabled())
        return true; // treat as handled to avoid spinning

    if (WiFi.status() != WL_CONNECTED)
        return true;

    unsigned long now = millis();
    if (suppressUntilMs_ && now < suppressUntilMs_)
        return true;

    if (now - lastAttemptMs_ < kMinPublishGapMs)
        return true;

    if (!haveTubeValue_ || !haveDoseValue_)
    {
        pendingPublish_ = false;
        return true;
    }

    payloadDoc_.clear();
    JsonArray arr = payloadDoc_.to<JsonArray>();
    JsonObject tubeObj = arr.add<JsonObject>();
    tubeObj["sensor"] = config_.openSenseTubeRateSensorId;
    tubeObj["value"] = pendingTubeValue_;
    JsonObject doseObj = arr.add<JsonObject>();
    doseObj["sensor"] = config_.openSenseDoseRateSensorId;
    doseObj["value"] = pendingDoseValue_;

    log_.print("OpenSenseMap: POST tube=");
    log_.print(pendingTubeValue_);
    log_.print(" dose=");
    log_.println(pendingDoseValue_);

    lastTlsErrorCode_ = 0;
    lastTlsErrorText_.clear();
    lastAttemptMs_ = now;
    bool ok = sendPayload(payloadDoc_);
    if (ok)
    {
        pendingPublish_ = false;
        haveTubeValue_ = false;
        haveDoseValue_ = false;
        lastAttemptMs_ = millis();
        consecutiveFailures_ = 0;
        suppressUntilMs_ = 0;
    }
    else
    {
        if (consecutiveFailures_ < 8)
            ++consecutiveFailures_;

        unsigned long backoff = kRetryBackoffMs;
        for (uint8_t i = 1; i < consecutiveFailures_; ++i)
        {
            if (backoff >= kMaxRetryBackoffMs)
                break;
            backoff = backoff * 2;
            if (backoff > kMaxRetryBackoffMs)
            {
                backoff = kMaxRetryBackoffMs;
                break;
            }
        }

        backoff += static_cast<unsigned long>(random(0, 1000)); // add small jitter
        suppressUntilMs_ = millis() + backoff;
        log_.print(F("OpenSenseMap: will retry in "));
        log_.print(backoff / 1000);
        log_.println(F("s"));

        if (lastTlsErrorCode_ == -0x0038 || lastTlsErrorCode_ == 56 || lastTlsErrorCode_ == -56)
        {
            log_.println(F("OpenSenseMap: TLS CTR_DRBG input-too-large; reboot or check Wi-Fi stability/time sync. This is an ESP32 mbedTLS quirk."));
        }
    }
    return true;
}

void OpenSenseMapPublisher::HandlePortalPost(WebServer &server,
                                             AppConfig &config,
                                             AppConfigStore &store,
                                             LedController &led,
                                             Print &log,
                                             String &message)
{
    bool enabled = server.hasArg("osemEnabled") && server.arg("osemEnabled") == "1";
    String boxId = server.arg("osemBoxId");
    String apiKey = server.arg("osemApiKey");
    String rateId = server.arg("osemRate");
    String doseId = server.arg("osemDose");

    boxId.trim();
    apiKey.trim();
    rateId.trim();
    doseId.trim();

    bool changed = false;
    if (config.openSenseMapEnabled != enabled)
    {
        config.openSenseMapEnabled = enabled;
        changed = true;
    }
    changed |= UpdateStringIfChanged(config.openSenseBoxId, boxId.c_str());
    changed |= UpdateStringIfChanged(config.openSenseApiKey, apiKey.c_str());
    changed |= UpdateStringIfChanged(config.openSenseTubeRateSensorId, rateId.c_str());
    changed |= UpdateStringIfChanged(config.openSenseDoseRateSensorId, doseId.c_str());

    if (changed)
    {
        if (store.save(config))
        {
            log.println("OpenSenseMap configuration updated via portal.");
            led.clearFault(FaultCode::NvsWriteFailure);
            message = F("OpenSenseMap settings saved.");
        }
        else
        {
            log.println("Preferences write failed; OpenSenseMap configuration not saved.");
            led.activateFault(FaultCode::NvsWriteFailure);
            message = F("Failed to save settings to NVS.");
        }
        return;
    }

    message = F("No changes detected.");
}

void OpenSenseMapPublisher::SendPortalForm(WiFiPortalService &portal, const String &message)
{
    if (!portal.manager_.server)
        return;

    String notice = WiFiPortalService::htmlEscape(message);
    WiFiPortalService::TemplateReplacements vars = {
        {"{{NOTICE_CLASS}}", notice.length() ? String() : String("hidden")},
        {"{{NOTICE_TEXT}}", notice},
        {"{{OSEM_ENABLED_CHECKED}}", portal.config_.openSenseMapEnabled ? String("checked") : String()},
        {"{{OSEM_BOX_ID}}", WiFiPortalService::htmlEscape(portal.config_.openSenseBoxId)},
        {"{{OSEM_API_KEY}}", WiFiPortalService::htmlEscape(portal.config_.openSenseApiKey)},
        {"{{OSEM_RATE_ID}}", WiFiPortalService::htmlEscape(portal.config_.openSenseTubeRateSensorId)},
        {"{{OSEM_DOSE_ID}}", WiFiPortalService::htmlEscape(portal.config_.openSenseDoseRateSensorId)}};

    portal.appendCommonTemplateVars(vars);
    portal.sendTemplate("/portal/osem.html", vars);
}

bool OpenSenseMapPublisher::sendPayload(const JsonDocument &payload)
{
    WiFiClientSecure client;
    client.setTimeout(10000);
    client.setCACert(kOpenSenseMapRootCa);

    if (!client.connect(kHost, kPort))
    {
        log_.println("OpenSenseMap: connect failed.");
        lastTlsErrorCode_ = logTlsError(log_, client, "connect", &lastTlsErrorText_);
        return false;
    }

    const size_t contentLength = measureJson(payload);

    // Request line and headers
    client.print(F("POST /boxes/"));
    client.print(config_.openSenseBoxId);
    client.println(F("/data HTTP/1.1"));
    client.print(F("Host: "));
    client.println(kHost);
    client.println(F("Connection: close"));
    client.println(F("Content-Type: application/json"));
    client.print(F("Content-Length: "));
    client.println(contentLength);
    client.print(F("Authorization: "));
    client.println(config_.openSenseApiKey);
    client.print(F("User-Agent: RadPro-WiFi-Bridge/"));
    client.println(bridgeVersion_);
    client.println();

    // Body
    serializeJson(payload, client);

    if (client.getWriteError())
    {
        log_.println("OpenSenseMap: failed to write request.");
        lastTlsErrorCode_ = logTlsError(log_, client, "write", &lastTlsErrorText_);
        return false;
    }

    // Read status line
    String status = client.readStringUntil('\n');
    status.trim();
    if (!status.startsWith("HTTP/1.1 "))
    {
        log_.print("OpenSenseMap: unexpected status line: ");
        log_.println(status.length() ? status : String("<empty>"));
        lastTlsErrorCode_ = logTlsError(log_, client, "status", &lastTlsErrorText_);
        return false;
    }

    const int statusCode = status.substring(9).toInt();
    if (statusCode < 200 || statusCode >= 300)
    {
        log_.print("OpenSenseMap: HTTP ");
        log_.println(statusCode);
        return false;
    }

    // Consume remainder
    while (client.connected() || client.available())
        client.read();

    return true;
}
