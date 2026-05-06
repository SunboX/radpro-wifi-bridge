#include "Safecast/SafecastPublisher.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <cmath>
#include <cstdlib>
#include <time.h>

#include "Publishing/HttpPublishResponse.h"
#include "Runtime/CooperativePump.h"
#include "Safecast/SafecastLogRedaction.h"
#include "Safecast/SafecastPayload.h"
#include "Safecast/SafecastProtocol.h"

namespace
{
constexpr unsigned long kResponseWaitMs = 15000;
constexpr unsigned long kTimeRetryBackoffMs = 10000;

bool parseMeasurementValue(const String &text, float &out)
{
    char *end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (!end || end == text.c_str() || *end != '\0' || !std::isfinite(parsed) || parsed < 0.0f)
        return false;
    out = parsed;
    return true;
}

template <typename Client>
String readResponseBody(Client &client, unsigned long timeoutMs, size_t maxBytes)
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

        CooperativePump::service();
    }

    body.trim();
    return body;
}
} // namespace

SafecastPublisher::SafecastPublisher(AppConfig &config,
                                     Print &log,
                                     const char *bridgeVersion,
                                     PublisherHealth &health)
    : config_(config),
      log_(log),
      bridgeVersion_(bridgeVersion ? bridgeVersion : ""),
      health_(health)
{
}

void SafecastPublisher::begin()
{
    updateConfig();
    syncHealthState();
}

void SafecastPublisher::updateConfig()
{
    String configError;
    if (config_.safecastEnabled)
    {
        SafecastConfig::ResolvedConfig resolved;
        const SafecastConfig::Error error = SafecastConfig::resolve(config_, resolved, true);
        if (error != SafecastConfig::Error::None)
            configError = SafecastConfig::errorText(error);
    }

    if (configError != lastConfigError_)
    {
        lastConfigError_ = configError;
        if (lastConfigError_.length())
        {
            log_.print("Safecast: ");
            log_.println(lastConfigError_);
        }
    }

    syncHealthState();
}

void SafecastPublisher::loop()
{
    syncHealthState();
    if (paused_)
        return;
    publishPending();
}

void SafecastPublisher::onCommandResult(DeviceManager::CommandType type, const String &value)
{
    float parsed = 0.0f;
    if (!parseMeasurementValue(value, parsed))
        return;

    switch (type)
    {
    case DeviceManager::CommandType::TubeRate:
        addSample(cpmWindow_, parsed, millis());
        break;
    case DeviceManager::CommandType::TubeDoseRate:
        addSample(doseWindow_, parsed, millis());
        break;
    default:
        break;
    }
    syncHealthState();
}

void SafecastPublisher::clearPendingData()
{
    cpmWindow_ = SampleWindow{};
    doseWindow_ = SampleWindow{};
    suppressUntilMs_ = 0;
    syncHealthState();
}

bool SafecastPublisher::isEnabled() const
{
    return config_.safecastEnabled && !lastConfigError_.length();
}

bool SafecastPublisher::publishPending()
{
    if (!isEnabled())
    {
        syncHealthState();
        return true;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        syncHealthState();
        return true;
    }

    SafecastConfig::ResolvedConfig resolved;
    const SafecastConfig::Error error = SafecastConfig::resolve(config_, resolved, true);
    if (error != SafecastConfig::Error::None)
    {
        syncHealthState();
        return true;
    }

    const unsigned long now = millis();
    const unsigned long intervalMs = resolved.uploadIntervalSeconds * 1000UL;
    if (suppressUntilMs_ && now < suppressUntilMs_)
    {
        syncHealthState();
        return true;
    }
    if (lastAttemptMs_ != 0 && now - lastAttemptMs_ < intervalMs)
    {
        syncHealthState();
        return true;
    }

    SafecastPayload::Measurement measurement;
    String buildError;
    if (!buildMeasurementFromAverage(resolved, measurement, buildError))
    {
        if (buildError.length())
        {
            log_.print("Safecast: ");
            log_.println(buildError);
        }
        suppressUntilMs_ = now + kTimeRetryBackoffMs;
        syncHealthState();
        return true;
    }

    lastAttemptMs_ = now;
    health_.noteAttempt(now);
    UploadResult result = uploadMeasurement(resolved, measurement, true);
    if (!result.success)
    {
        const unsigned long retryBackoffMs = intervalMs < 60000UL ? 60000UL : intervalMs;
        suppressUntilMs_ = millis() + retryBackoffMs;
    }
    else
    {
        suppressUntilMs_ = 0;
    }

    syncHealthState();
    return true;
}

SafecastPublisher::UploadResult SafecastPublisher::sendTestUpload(const AppConfig &configOverride)
{
    UploadResult result;
    if (WiFi.status() != WL_CONNECTED)
    {
        result.errorMessage = "Wi-Fi is not connected.";
        return result;
    }

    SafecastConfig::ResolvedConfig resolved;
    const SafecastConfig::Error error = SafecastConfig::resolve(configOverride, resolved, true);
    if (error != SafecastConfig::Error::None)
    {
        result.errorMessage = SafecastConfig::errorText(error);
        return result;
    }

    SafecastPayload::Measurement measurement;
    String buildError;
    if (!buildMeasurementFromLatest(resolved, measurement, buildError))
    {
        result.errorMessage = buildError.length() ? buildError : String("No valid detector reading available for Safecast test upload.");
        return result;
    }

    return uploadMeasurement(resolved, measurement, false);
}

void SafecastPublisher::syncHealthState()
{
    health_.setEnabled(config_.safecastEnabled);
    health_.setPaused(paused_ || (config_.safecastEnabled && lastConfigError_.length()));
    const bool pending = config_.safecastUnit.equalsIgnoreCase("usv")
                             ? doseWindow_.hasLatest
                             : cpmWindow_.hasLatest;
    health_.setPending(config_.safecastEnabled && pending);
}

bool SafecastPublisher::makeIsoTimestamp(String &out) const
{
    const time_t now = time(nullptr);
    if (now <= 0)
        return false;

    struct tm tmUtc;
    if (!gmtime_r(&now, &tmUtc))
        return false;

    char buffer[32];
    const size_t written = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
    if (written == 0)
        return false;

    out = buffer;
    return true;
}

void SafecastPublisher::addSample(SampleWindow &window, float value, unsigned long now)
{
    RateSample sample;
    sample.timestampMs = now;
    sample.value = value;
    window.samples.push_back(sample);
    window.sum += value;
    window.hasLatest = true;
    window.latestValue = value;
}

void SafecastPublisher::pruneSamples(SampleWindow &window, unsigned long now, unsigned long windowMs)
{
    while (!window.samples.empty())
    {
        const unsigned long age = now - window.samples.front().timestampMs;
        if (age <= windowMs)
            break;
        window.sum -= window.samples.front().value;
        window.samples.erase(window.samples.begin());
    }

    if (window.samples.empty())
        window.sum = 0.0f;
    else if (window.sum < 0.0f)
        window.sum = 0.0f;
}

bool SafecastPublisher::buildMeasurementFromAverage(const SafecastConfig::ResolvedConfig &resolved,
                                                    SafecastPayload::Measurement &outMeasurement,
                                                    String &outError)
{
    String capturedAt;
    if (!makeIsoTimestamp(capturedAt))
    {
        outError = "waiting for valid system time before uploading";
        return false;
    }

    SampleWindow &window = resolved.unit == "usv" ? doseWindow_ : cpmWindow_;
    const unsigned long windowMs = resolved.uploadIntervalSeconds * 1000UL;
    pruneSamples(window, millis(), windowMs);
    if (window.samples.empty())
    {
        outError = "no valid measurement available for upload";
        return false;
    }

    outMeasurement = SafecastPayload::Measurement{};
    outMeasurement.hasLatitude = true;
    outMeasurement.hasLongitude = true;
    outMeasurement.hasValue = true;
    outMeasurement.latitude = resolved.latitude;
    outMeasurement.longitude = resolved.longitude;
    outMeasurement.value = window.sum / static_cast<float>(window.samples.size());
    outMeasurement.unit = resolved.unit;
    outMeasurement.capturedAt = capturedAt;
    outMeasurement.locationName = resolved.locationName;
    if (resolved.hasDeviceId)
        outMeasurement.deviceId = String(resolved.deviceId);
    if (resolved.hasHeightCm)
        outMeasurement.heightCm = String(resolved.heightCm);

    return true;
}

bool SafecastPublisher::buildMeasurementFromLatest(const SafecastConfig::ResolvedConfig &resolved,
                                                   SafecastPayload::Measurement &outMeasurement,
                                                   String &outError)
{
    String capturedAt;
    if (!makeIsoTimestamp(capturedAt))
    {
        outError = "Waiting for valid system time before test upload.";
        return false;
    }

    const SampleWindow &window = resolved.unit == "usv" ? doseWindow_ : cpmWindow_;
    if (!window.hasLatest)
    {
        outError = "No current detector reading is available for the selected unit.";
        return false;
    }

    outMeasurement = SafecastPayload::Measurement{};
    outMeasurement.hasLatitude = true;
    outMeasurement.hasLongitude = true;
    outMeasurement.hasValue = true;
    outMeasurement.latitude = resolved.latitude;
    outMeasurement.longitude = resolved.longitude;
    outMeasurement.value = window.latestValue;
    outMeasurement.unit = resolved.unit;
    outMeasurement.capturedAt = capturedAt;
    outMeasurement.locationName = resolved.locationName;
    if (resolved.hasDeviceId)
        outMeasurement.deviceId = String(resolved.deviceId);
    if (resolved.hasHeightCm)
        outMeasurement.heightCm = String(resolved.heightCm);

    return true;
}

SafecastPublisher::UploadResult SafecastPublisher::uploadMeasurement(const SafecastConfig::ResolvedConfig &resolved,
                                                                     const SafecastPayload::Measurement &measurement,
                                                                     bool updateHealthState)
{
    UploadResult result;

    JsonDocument doc;
    const SafecastPayload::BuildError buildError = SafecastPayload::buildPayloadDocument(doc, measurement);
    if (buildError != SafecastPayload::BuildError::None)
    {
        result.errorMessage = SafecastPayload::buildErrorText(buildError);
        return result;
    }

    String payload;
    serializeJson(doc, payload);

    if (resolved.debug)
    {
        const String redactedUrl = SafecastLogRedaction::redactUrlForLogs(
            SafecastProtocol::buildMeasurementUrl(resolved.resolvedBaseUrl, resolved.apiKey));
        log_.print("Safecast: POST ");
        log_.println(redactedUrl);
        log_.print("Safecast: payload ");
        log_.println(payload);
    }

    if (resolved.endpoint.secure)
    {
        WiFiClientSecure client;
        client.setTimeout(kResponseWaitMs / 1000);
        client.setInsecure();
        return sendRequest(client, resolved, payload, updateHealthState);
    }

    WiFiClient client;
    client.setTimeout(kResponseWaitMs / 1000);
    return sendRequest(client, resolved, payload, updateHealthState);
}

template <typename Client>
SafecastPublisher::UploadResult SafecastPublisher::sendRequest(Client &client,
                                                               const SafecastConfig::ResolvedConfig &resolved,
                                                               const String &payload,
                                                               bool updateHealthState)
{
    UploadResult result;
    result.redactedUrl = SafecastLogRedaction::redactUrlForLogs(
        SafecastProtocol::buildMeasurementUrl(resolved.resolvedBaseUrl, resolved.apiKey));
    result.attempted = true;

    if (!client.connect(resolved.endpoint.host.c_str(), resolved.endpoint.port))
    {
        result.errorMessage = "connect failed";
        if (updateHealthState)
            health_.noteFailure(millis(), result.errorMessage);
        log_.println("Safecast: connect failed.");
        return result;
    }

    const String requestPath = SafecastProtocol::buildMeasurementPath(resolved.endpoint, resolved.apiKey);

    String request;
    request += "POST ";
    request += requestPath;
    request += " HTTP/1.1\r\nHost: ";
    request += resolved.endpoint.host;
    request += "\r\nConnection: close\r\nContent-Type: ";
    request += SafecastProtocol::kContentType;
    request += "\r\nAccept: application/json\r\nContent-Length: ";
    request += payload.length();
    request += "\r\nUser-Agent: RadPro-WiFi-Bridge/";
    request += bridgeVersion_;
    request += "\r\n\r\n";
    request += payload;

    if (client.print(request) != request.length())
    {
        result.errorMessage = "send failed";
        if (updateHealthState)
            health_.noteFailure(millis(), result.errorMessage);
        log_.println("Safecast: failed to write request.");
        return result;
    }

    client.flush();

    const auto response = HttpPublishResponse::readStatus(
        client,
        kResponseWaitMs,
        []() { return millis(); },
        []() { CooperativePump::service(); });

    result.statusCode = response.statusCode;
    result.statusLine = response.statusLine;
    result.responseBody = readResponseBody(client, 1500, 512);

    if (!response.success)
    {
        if (!result.statusCode && !response.statusLine.length())
            result.errorMessage = "no response";
        else if (!result.responseBody.length())
            result.errorMessage = response.failure == HttpPublishResponse::FailureKind::HttpError ? String("http error") : String(HttpPublishResponse::failureText(response.failure));
        else
            result.errorMessage = result.responseBody;

        if (response.failure == HttpPublishResponse::FailureKind::HttpError)
        {
            log_.print("Safecast: HTTP ");
            log_.println(response.statusCode);
            if (resolved.debug && result.responseBody.length())
            {
                log_.print("Safecast: response body: ");
                log_.println(result.responseBody);
            }
        }
        else
        {
            log_.print("Safecast: ");
            log_.println(HttpPublishResponse::failureText(response.failure));
        }

        if (updateHealthState)
            health_.noteFailure(millis(),
                                result.errorMessage.length() ? result.errorMessage : String(HttpPublishResponse::failureText(response.failure)),
                                response.statusCode,
                                response.statusLine,
                                result.responseBody.length() ? result.responseBody : response.trace);
        return result;
    }

    result.success = true;
    if (resolved.debug && result.responseBody.length())
    {
        log_.print("Safecast: response body: ");
        log_.println(result.responseBody);
    }

    if (updateHealthState)
        health_.noteSuccess(millis(), response.statusCode, response.statusLine);

    while (client.connected() || client.available())
        client.read();

    return result;
}
