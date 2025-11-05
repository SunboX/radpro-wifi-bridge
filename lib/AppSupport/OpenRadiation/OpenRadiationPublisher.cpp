#include "OpenRadiation/OpenRadiationPublisher.h"

#include <WiFi.h>
#include <time.h>
#include <esp_system.h>
#include <cmath>

namespace
{
    constexpr const char *kHost = "submit.open-radiation.net";
    constexpr const char *kPath = "/measurements";
    constexpr uint16_t kPort = 443;
    constexpr unsigned long kMinPublishGapMs = 60000;
    constexpr unsigned long kRetryBackoffMs = 120000;
}

OpenRadiationPublisher::OpenRadiationPublisher(AppConfig &config, Print &log, const char *bridgeVersion)
    : config_(config),
      log_(log),
      bridgeVersion_(bridgeVersion ? bridgeVersion : "")
{
}

void OpenRadiationPublisher::begin()
{
    updateConfig();
}

void OpenRadiationPublisher::updateConfig()
{
    // Placeholder for future dynamic reactions to config changes.
}

void OpenRadiationPublisher::loop()
{
    publishPending();
}

void OpenRadiationPublisher::onCommandResult(DeviceManager::CommandType type, const String &value)
{
    switch (type)
    {
    case DeviceManager::CommandType::TubeRate:
        if (value.length())
        {
            pendingTubeValue_ = value;
            haveTubeValue_ = true;
        }
        break;
    case DeviceManager::CommandType::TubeDoseRate:
        if (value.length())
        {
            pendingDoseValue_ = value;
            haveDoseValue_ = true;
            if (haveTubeValue_)
            {
                publishQueued_ = true;
                suppressUntilMs_ = 0;
            }
        }
        break;
    default:
        break;
    }
}

bool OpenRadiationPublisher::isEnabled() const
{
    if (!config_.openRadiationEnabled)
        return false;
    if (!config_.openRadiationDeviceId.length())
        return false;
    if (!config_.openRadiationApiKey.length())
        return false;
    return true;
}

bool OpenRadiationPublisher::publishPending()
{
    if (!publishQueued_)
        return false;

    if (!isEnabled())
        return true;

    if (!haveDoseValue_)
        return true;

    if (WiFi.status() != WL_CONNECTED)
        return true;

    unsigned long now = millis();
    if (suppressUntilMs_ && now < suppressUntilMs_)
        return true;

    if (now - lastAttemptMs_ < kMinPublishGapMs)
        return true;

    float doseRate = pendingDoseValue_.toFloat();
    if (!(doseRate > 0.0f))
    {
        log_.println("OpenRadiation: invalid dose value; aborting publish.");
        publishQueued_ = false;
        haveDoseValue_ = false;
        haveTubeValue_ = false;
        return true;
    }

    int hitCount = -1;
    if (haveTubeValue_ && pendingTubeValue_.length())
    {
        hitCount = lroundf(pendingTubeValue_.toFloat());
        if (hitCount < 0)
            hitCount = 0;
    }

    String timestamp;
    if (!makeIsoTimestamp(timestamp))
    {
        log_.println("OpenRadiation: waiting for valid system time before publishing.");
        suppressUntilMs_ = now + 10000;
        return true;
    }

    if (fabsf(config_.openRadiationLatitude) < 0.000001f && fabsf(config_.openRadiationLongitude) < 0.000001f)
    {
        log_.println("OpenRadiation: latitude/longitude not configured; skipping publish.");
        suppressUntilMs_ = now + kRetryBackoffMs;
        return true;
    }

    String payload;
    if (!buildPayload(payload, doseRate, hitCount, timestamp))
    {
        log_.println("OpenRadiation: failed to build payload.");
        suppressUntilMs_ = now + kRetryBackoffMs;
        return true;
    }

    log_.print("OpenRadiation: POST dose=");
    log_.print(doseRate, 4);
    if (hitCount >= 0)
    {
        log_.print(" hits=");
        log_.print(hitCount);
    }
    log_.println();

    lastAttemptMs_ = now;
    bool ok = sendPayload(payload);
    if (ok)
    {
        publishQueued_ = false;
        haveDoseValue_ = false;
        haveTubeValue_ = false;
        lastAttemptMs_ = millis();
    }
    else
    {
        suppressUntilMs_ = millis() + kRetryBackoffMs;
    }
    return true;
}

bool OpenRadiationPublisher::buildPayload(String &outJson, float doseRate, int hitCount, String &timestamp)
{
    if (!config_.openRadiationDeviceId.length() || !config_.openRadiationApiKey.length())
        return false;

    outJson.reserve(512);
    outJson = "{\"apiKey\":\"";
    outJson += config_.openRadiationApiKey;
    outJson += "\",\"data\":{";
    outJson += "\"apparatusId\":\"";
    outJson += config_.openRadiationDeviceId;
    outJson += "\",\"value\":";
    outJson += String(doseRate, 4);
    if (hitCount >= 0)
    {
        outJson += ",\"hitsNumber\":";
        outJson += String(hitCount);
    }
    outJson += ",\"startTime\":\"";
    outJson += timestamp;
    outJson += "\",\"latitude\":";
    outJson += String(config_.openRadiationLatitude, 6);
    outJson += ",\"longitude\":";
    outJson += String(config_.openRadiationLongitude, 6);
    if (config_.openRadiationAltitude != 0.0f)
    {
        outJson += ",\"altitude\":";
        outJson += String(config_.openRadiationAltitude, 1);
    }
    if (config_.openRadiationAccuracy > 0.0f)
    {
        outJson += ",\"accuracy\":";
        outJson += String(config_.openRadiationAccuracy, 1);
    }
    outJson += ",\"reportUuid\":\"";
    outJson += generateUuid();
    outJson += "\",\"reportContext\":\"routine\"";
    outJson += "}}";
    return true;
}

bool OpenRadiationPublisher::sendPayload(const String &payload)
{
    WiFiClientSecure client;
    client.setTimeout(15000);
    client.setInsecure();

    if (!client.connect(kHost, kPort))
    {
        log_.println("OpenRadiation: connect failed.");
        return false;
    }

    String request;
    request.reserve(payload.length() + 240);
    request += "POST ";
    request += kPath;
    request += " HTTP/1.1\r\nHost: ";
    request += kHost;
    request += "\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: ";
    request += payload.length();
    request += "\r\nUser-Agent: RadPro-WiFi-Bridge/";
    request += bridgeVersion_;
    request += "\r\n\r\n";
    request += payload;

    if (client.print(request) != request.length())
    {
        log_.println("OpenRadiation: failed to write request.");
        return false;
    }

    String status = client.readStringUntil('\n');
    status.trim();
    if (!status.startsWith("HTTP/1.1 "))
    {
        log_.print("OpenRadiation: unexpected status line: ");
        log_.println(status);
        return false;
    }

    int statusCode = status.substring(9).toInt();
    if (statusCode < 200 || statusCode >= 300)
    {
        log_.print("OpenRadiation: HTTP ");
        log_.println(statusCode);
        return false;
    }

    while (client.connected())
        client.read();

    return true;
}

bool OpenRadiationPublisher::makeIsoTimestamp(String &out) const
{
    time_t now = time(nullptr);
    if (now <= 0)
        return false;

    struct tm tmUtc;
    if (!gmtime_r(&now, &tmUtc))
        return false;

    char buffer[32];
    size_t written = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
    if (written == 0)
        return false;

    out = buffer;
    return true;
}

String OpenRadiationPublisher::generateUuid()
{
    uint8_t bytes[16];
    esp_fill_random(bytes, sizeof(bytes));

    bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3F) | 0x80);

    char buffer[37];
    snprintf(buffer, sizeof(buffer),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3],
             bytes[4], bytes[5], bytes[6], bytes[7],
             bytes[8], bytes[9], bytes[10], bytes[11],
             bytes[12], bytes[13], bytes[14], bytes[15]);

    return String(buffer);
}
