#include "OpenRadiation/OpenRadiationPublisher.h"
#include "OpenRadiation/OpenRadiationMeasurementMetadata.h"
#include "OpenRadiation/OpenRadiationMeasurementWindow.h"
#include "OpenRadiation/OpenRadiationProtocol.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>
#include <esp_system.h>
#include <cmath>
#include "Publishing/HttpPublishResponse.h"

namespace
{
    constexpr const char *kPath = "/measurements";
    constexpr uint16_t kPort = 443;
    constexpr unsigned long kMinPublishGapMs = 60000;
    constexpr unsigned long kRetryBackoffMs = 120000;
    constexpr unsigned long kResponseWaitMs = 15000;
}

OpenRadiationPublisher::OpenRadiationPublisher(AppConfig &config,
                                               DeviceInfoStore &deviceInfo,
                                               Print &log,
                                               const char *bridgeVersion,
                                               PublisherHealth &health)
    : config_(config),
      deviceInfo_(deviceInfo),
      log_(log),
      bridgeVersion_(bridgeVersion ? bridgeVersion : ""),
      health_(health)
{
}

void OpenRadiationPublisher::begin()
{
    updateConfig();
    syncHealthState();
}

void OpenRadiationPublisher::updateConfig()
{
    // Placeholder for future dynamic reactions to config changes.
    syncHealthState();
}

void OpenRadiationPublisher::loop()
{
    syncHealthState();
    publishPending();
}

void OpenRadiationPublisher::clearPendingData()
{
    pendingDoseValue_ = String();
    pendingTubeValue_ = String();
    OpenRadiationMeasurementWindow::clearMeasurementWindow(measurementWindow_);
    haveDoseValue_ = false;
    haveTubeValue_ = false;
    publishQueued_ = false;
    suppressUntilMs_ = 0;
    syncHealthState();
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

                String queuedTimestamp;
                const DeviceInfoSnapshot info = deviceInfo_.snapshot();
                if (makeIsoTimestamp(queuedTimestamp))
                    OpenRadiationMeasurementWindow::replaceMeasurementWindow(
                        measurementWindow_,
                        queuedTimestamp,
                        info.tubePulseCount);
                else
                    OpenRadiationMeasurementWindow::replaceMeasurementWindow(
                        measurementWindow_,
                        String(),
                        String());
            }
        }
        break;
    default:
        break;
    }
    syncHealthState();
}

bool OpenRadiationPublisher::isEnabled() const
{
    if (!config_.openRadiationEnabled)
        return false;
    if (!resolveApparatusId().length())
        return false;
    if (!config_.openRadiationApiKey.length())
        return false;
    return true;
}

String OpenRadiationPublisher::resolveApparatusId() const
{
    const DeviceInfoSnapshot info = deviceInfo_.snapshot();
    return OpenRadiationProtocol::resolveApparatusId(config_.openRadiationDeviceId, info.deviceId);
}

bool OpenRadiationPublisher::publishPending()
{
    if (!publishQueued_)
    {
        syncHealthState();
        return false;
    }

    if (!isEnabled())
    {
        syncHealthState();
        return true;
    }

    if (!haveDoseValue_)
    {
        syncHealthState();
        return true;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        syncHealthState();
        return true;
    }

    unsigned long now = millis();
    if (suppressUntilMs_ && now < suppressUntilMs_)
    {
        syncHealthState();
        return true;
    }

    if (now - lastAttemptMs_ < kMinPublishGapMs)
    {
        syncHealthState();
        return true;
    }

    float doseRate = pendingDoseValue_.toFloat();
    if (!(doseRate > 0.0f))
    {
        log_.println("OpenRadiation: invalid dose value; aborting publish.");
        publishQueued_ = false;
        haveDoseValue_ = false;
        haveTubeValue_ = false;
        OpenRadiationMeasurementWindow::clearMeasurementWindow(measurementWindow_);
        syncHealthState();
        return true;
    }

    String endTimestamp;
    if (!makeIsoTimestamp(endTimestamp))
    {
        log_.println("OpenRadiation: waiting for valid system time before publishing.");
        suppressUntilMs_ = now + 10000;
        syncHealthState();
        return true;
    }

    const DeviceInfoSnapshot info = deviceInfo_.snapshot();
    if (!OpenRadiationMeasurementWindow::hasMeasurementWindow(measurementWindow_))
    {
        OpenRadiationMeasurementWindow::armMeasurementWindow(
            measurementWindow_,
            endTimestamp,
            info.tubePulseCount);
    }

    if (fabsf(config_.openRadiationLatitude) < 0.000001f && fabsf(config_.openRadiationLongitude) < 0.000001f)
    {
        log_.println("OpenRadiation: latitude/longitude not configured; skipping publish.");
        suppressUntilMs_ = now + kRetryBackoffMs;
        syncHealthState();
        return true;
    }

    String payload;
    String reportUuid;
    if (!buildPayload(payload,
                      reportUuid,
                      doseRate,
                      measurementWindow_.startTime,
                      endTimestamp,
                      measurementWindow_.startPulseCount,
                      info.tubePulseCount))
    {
        log_.println("OpenRadiation: failed to build payload.");
        suppressUntilMs_ = now + kRetryBackoffMs;
        syncHealthState();
        return true;
    }

    const String apparatusId = resolveApparatusId();
    log_.print("OpenRadiation: POST dose=");
    log_.print(doseRate, 4);
    log_.print(" apparatusId=");
    log_.print(apparatusId);
    log_.print(" reportUuid=");
    log_.print(reportUuid);
    log_.println();

    lastAttemptMs_ = now;
    health_.noteAttempt(now);
    bool ok = sendPayload(payload);
    if (ok)
    {
        lastPublishedReportUuid_ = reportUuid;
        publishQueued_ = false;
        haveDoseValue_ = false;
        haveTubeValue_ = false;
        OpenRadiationMeasurementWindow::clearMeasurementWindow(measurementWindow_);
        lastAttemptMs_ = millis();
    }
    else
    {
        suppressUntilMs_ = millis() + kRetryBackoffMs;
    }
    syncHealthState();
    return true;
}

bool OpenRadiationPublisher::buildPayload(String &outJson,
                                          String &outReportUuid,
                                          float doseRate,
                                          const String &startTime,
                                          const String &endTime,
                                          const String &startPulseCount,
                                          const String &endPulseCount)
{
    const String apparatusId = resolveApparatusId();
    if (!apparatusId.length() || !config_.openRadiationApiKey.length() || !startTime.length())
        return false;

    outReportUuid = generateUuid();

    const DeviceInfoSnapshot info = deviceInfo_.snapshot();

    JsonDocument doc;
    doc["apiKey"] = config_.openRadiationApiKey;

    JsonObject data = doc["data"].to<JsonObject>();
    data["apparatusId"] = apparatusId;
    data["value"] = doseRate;
    data["startTime"] = startTime;
    data["latitude"] = config_.openRadiationLatitude;
    data["longitude"] = config_.openRadiationLongitude;
    if (config_.openRadiationAltitude != 0.0f)
        data["altitude"] = lroundf(config_.openRadiationAltitude);
    if (config_.openRadiationAccuracy > 0.0f)
        data["accuracy"] = config_.openRadiationAccuracy;
    if (info.firmware.length())
        data["apparatusVersion"] = info.firmware;
    data["apparatusSensorType"] = "geiger";
    data["reportUuid"] = outReportUuid;
    data["manualReporting"] = false;
    data["organisationReporting"] = OpenRadiationProtocol::buildOrganisationReporting(bridgeVersion_);
    data["reportContext"] = "routine";
    OpenRadiationMeasurementMetadata::appendOptionalFields(
        data,
        config_,
        endTime,
        startPulseCount,
        endPulseCount);

    outJson.clear();
    serializeJson(doc, outJson);
    return true;
}

bool OpenRadiationPublisher::sendPayload(const String &payload)
{
    WiFiClientSecure client;
    client.setTimeout(15000);
    client.setInsecure();

    if (!client.connect(OpenRadiationProtocol::kSubmitHost, kPort))
    {
        log_.println("OpenRadiation: connect failed.");
        health_.noteFailure(millis(), "connect failed");
        return false;
    }

    String request;
    request.reserve(payload.length() + 240);
    request += "POST ";
    request += kPath;
    request += " HTTP/1.1\r\nHost: ";
    request += OpenRadiationProtocol::kSubmitHost;
    request += "\r\nConnection: close\r\nContent-Type: ";
    request += OpenRadiationProtocol::kContentType;
    request += "\r\nAccept: ";
    request += OpenRadiationProtocol::kAccept;
    request += "\r\nContent-Length: ";
    request += payload.length();
    request += "\r\nUser-Agent: RadPro-WiFi-Bridge/";
    request += bridgeVersion_;
    request += "\r\n\r\n";
    request += payload;

    if (client.print(request) != request.length())
    {
        log_.println("OpenRadiation: failed to write request.");
        health_.noteFailure(millis(), "send failed");
        return false;
    }

    client.flush();

    const auto response = HttpPublishResponse::readStatus(
        client,
        kResponseWaitMs,
        []() { return millis(); },
        []() {
            delay(10);
            yield();
        });

    if (!response.success)
    {
        String body = readResponseBody(client, 1500, 320);
        switch (response.failure)
        {
        case HttpPublishResponse::FailureKind::NoResponse:
            log_.print("OpenRadiation: no response before ");
            if (client.connected())
                log_.println("timeout");
            else
                log_.println("disconnect");
            health_.noteFailure(millis(), "no response", 0, response.statusLine, response.trace);
            break;
        case HttpPublishResponse::FailureKind::InvalidStatusLine:
            log_.print("OpenRadiation: unexpected status line: ");
            log_.println(response.statusLine.length() ? response.statusLine : String("<empty>"));
            if (response.trace.length())
            {
                log_.print("OpenRadiation: response trace: ");
                log_.println(response.trace);
            }
            health_.noteFailure(millis(), "invalid status line", 0, response.statusLine, response.trace);
            break;
        case HttpPublishResponse::FailureKind::HttpError:
            log_.print("OpenRadiation: HTTP ");
            log_.println(response.statusCode);
            if (body.length())
            {
                log_.print("OpenRadiation: response body: ");
                log_.println(body);
            }
            health_.noteFailure(millis(), body.length() ? body : String("http error"), response.statusCode, response.statusLine, body.length() ? body : response.trace);
            break;
        case HttpPublishResponse::FailureKind::ReadError:
            log_.println("OpenRadiation: response read error.");
            health_.noteFailure(millis(), "read error", 0, response.statusLine, response.trace);
            break;
        case HttpPublishResponse::FailureKind::None:
            break;
        }
        return false;
    }

    health_.noteSuccess(millis(), response.statusCode, response.statusLine);

    while (client.connected() || client.available())
        client.read();

    return true;
}

String OpenRadiationPublisher::readResponseBody(WiFiClientSecure &client, unsigned long timeoutMs, size_t maxBytes) const
{
    String body;
    const unsigned long startedAt = millis();
    while ((millis() - startedAt) < timeoutMs)
    {
        while (client.available() > 0)
        {
            const int ch = client.read();
            if (ch < 0)
                break;
            if (body.length() < maxBytes)
            {
                const char text[2] = {static_cast<char>(ch), '\0'};
                body += text;
            }
        }

        if (!client.connected() && client.available() <= 0)
            break;

        delay(10);
        yield();
    }

    body.trim();
    return body;
}

void OpenRadiationPublisher::syncHealthState()
{
    health_.setEnabled(isEnabled());
    health_.setPaused(false);
    health_.setPending(publishQueued_);
    health_.setLastReportUuid(lastPublishedReportUuid_);
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
