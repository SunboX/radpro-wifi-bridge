#include "OpenSenseMap/OpenSenseMapPublisher.h"

#include <WiFi.h>

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
}

OpenSenseMapPublisher::OpenSenseMapPublisher(AppConfig &config, Print &log)
    : config_(config),
      log_(log)
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
    publishPending();
}

void OpenSenseMapPublisher::onCommandResult(DeviceManager::CommandType type, const String &value)
{
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

    String payload;
    payload.reserve(256);
    payload = "[{\"sensor\":\"";
    payload += escapeJson(config_.openSenseTubeRateSensorId);
    payload += "\",\"value\":\"";
    payload += escapeJson(pendingTubeValue_);
    payload += "\"},{\"sensor\":\"";
    payload += escapeJson(config_.openSenseDoseRateSensorId);
    payload += "\",\"value\":\"";
    payload += escapeJson(pendingDoseValue_);
    payload += "\"}]";

    log_.print("OpenSenseMap: POST tube=");
    log_.print(pendingTubeValue_);
    log_.print(" dose=");
    log_.println(pendingDoseValue_);

    lastAttemptMs_ = now;
    bool ok = sendPayload(payload);
    if (ok)
    {
        pendingPublish_ = false;
        haveTubeValue_ = false;
        haveDoseValue_ = false;
        lastAttemptMs_ = millis();
    }
    else
    {
        suppressUntilMs_ = millis() + kRetryBackoffMs;
    }
    return true;
}

bool OpenSenseMapPublisher::sendPayload(const String &payload)
{
    WiFiClientSecure client;
    client.setTimeout(10000);
    client.setCACert(kOpenSenseMapRootCa);

    if (!client.connect(kHost, kPort))
    {
        log_.println("OpenSenseMap: connect failed.");
        return false;
    }

    String path = "/boxes/" + config_.openSenseBoxId + "/data";
    String request;
    request.reserve(payload.length() + 200);
    request += "POST ";
    request += path;
    request += " HTTP/1.1\r\nHost: ";
    request += kHost;
    request += "\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: ";
    request += payload.length();
    request += "\r\n";
    request += "Authorization: ";
    request += config_.openSenseApiKey;
    request += "\r\n\r\n";
    request += payload;

    if (client.print(request) != request.length())
    {
        log_.println("OpenSenseMap: failed to write request.");
        return false;
    }

    // Read status line
    String status = client.readStringUntil('\n');
    status.trim();
    if (!status.startsWith("HTTP/1.1 "))
    {
        log_.print("OpenSenseMap: unexpected status line: ");
        log_.println(status);
        return false;
    }

    int statusCode = status.substring(9).toInt();
    if (statusCode < 200 || statusCode >= 300)
    {
        log_.print("OpenSenseMap: HTTP ");
        log_.println(statusCode);
        return false;
    }

    // Consume remainder
    while (client.connected())
        client.read();

    return true;
}

String OpenSenseMapPublisher::escapeJson(const String &value)
{
    String out;
    out.reserve(value.length());
    for (size_t i = 0; i < value.length(); ++i)
    {
        char c = value[i];
        switch (c)
        {
        case '\\':
        case '"':
            out += '\\';
            out += c;
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char buf[7];
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                out += buf;
            }
            else
            {
                out += c;
            }
            break;
        }
    }
    return out;
}
