#pragma once

#include "AppConfig/AppConfig.h"
#include "Safecast/SafecastPayload.h"
#include "Safecast/SafecastProtocol.h"

#include <cmath>
#include <cstdlib>

namespace SafecastConfig
{
static constexpr uint32_t kMinUploadIntervalSeconds = 60;
static constexpr uint32_t kDefaultUploadIntervalSeconds = 300;

enum class Error
{
    None,
    MissingApiKey,
    MissingLatitude,
    MissingLongitude,
    InvalidLatitude,
    InvalidLongitude,
    InvalidUploadInterval,
    InvalidUnit,
    InvalidDeviceId,
    InvalidHeightCm,
    InvalidApiBaseUrl,
    InvalidCustomApiBaseUrl,
};

struct ResolvedConfig
{
    bool enabled = false;
    bool debug = false;
    uint32_t uploadIntervalSeconds = kDefaultUploadIntervalSeconds;
    float latitude = 0.0f;
    float longitude = 0.0f;
    bool hasDeviceId = false;
    long deviceId = 0;
    bool hasHeightCm = false;
    long heightCm = 0;
    String apiKey;
    String locationName;
    String unit;
    String resolvedBaseUrl;
    SafecastProtocol::Endpoint endpoint;
};

inline bool parseFloatText(const String &text, float &out)
{
    String trimmed = text;
    trimmed.trim();
    if (!trimmed.length())
        return false;

    char *end = nullptr;
    const float parsed = std::strtof(trimmed.c_str(), &end);
    if (!end || end == trimmed.c_str() || *end != '\0' || !std::isfinite(parsed))
        return false;
    out = parsed;
    return true;
}

inline const char *errorText(Error error)
{
    switch (error)
    {
    case Error::None:
        return "";
    case Error::MissingApiKey:
        return "Safecast API key is required.";
    case Error::MissingLatitude:
        return "Latitude is required.";
    case Error::MissingLongitude:
        return "Longitude is required.";
    case Error::InvalidLatitude:
        return "Latitude must be between -90 and 90.";
    case Error::InvalidLongitude:
        return "Longitude must be between -180 and 180.";
    case Error::InvalidUploadInterval:
        return "Upload interval must be at least 60 seconds.";
    case Error::InvalidUnit:
        return "Unit must be either cpm or usv.";
    case Error::InvalidDeviceId:
        return "Device ID must be numeric.";
    case Error::InvalidHeightCm:
        return "Height above ground must be numeric.";
    case Error::InvalidApiBaseUrl:
        return "API endpoint URL is invalid.";
    case Error::InvalidCustomApiBaseUrl:
        return "Custom endpoint URL is invalid.";
    }
    return "Invalid Safecast configuration.";
}

inline Error resolve(const AppConfig &config, ResolvedConfig &out, bool requirePublishableFields)
{
    out = ResolvedConfig{};
    out.enabled = config.safecastEnabled;
    out.debug = config.safecastDebug;
    out.uploadIntervalSeconds = config.safecastUploadIntervalSeconds;
    out.apiKey = config.safecastApiKey;
    out.apiKey.trim();
    out.locationName = config.safecastLocationName;
    out.locationName.trim();
    out.unit = SafecastPayload::normalizeUnit(config.safecastUnit);

    if (!out.unit.length())
        return Error::InvalidUnit;
    if (out.uploadIntervalSeconds < kMinUploadIntervalSeconds)
        return Error::InvalidUploadInterval;

    if (config.safecastDeviceId.length())
    {
        if (!SafecastPayload::parseWholeNumber(config.safecastDeviceId, out.deviceId) || out.deviceId < 0)
            return Error::InvalidDeviceId;
        out.hasDeviceId = true;
    }

    if (config.safecastHeightCm.length())
    {
        if (!SafecastPayload::parseWholeNumber(config.safecastHeightCm, out.heightCm) || out.heightCm < 0)
            return Error::InvalidHeightCm;
        out.hasHeightCm = true;
    }

    const String productionBaseUrl = config.safecastApiBaseUrl.length()
                                         ? config.safecastApiBaseUrl
                                         : String(SafecastProtocol::kProductionApiBaseUrl);
    SafecastProtocol::Endpoint productionEndpoint;
    if (!SafecastProtocol::parseBaseUrl(productionBaseUrl, productionEndpoint))
        return Error::InvalidApiBaseUrl;

    if (config.safecastCustomApiBaseUrl.length())
    {
        SafecastProtocol::Endpoint customEndpoint;
        if (!SafecastProtocol::parseBaseUrl(config.safecastCustomApiBaseUrl, customEndpoint))
            return Error::InvalidCustomApiBaseUrl;
    }

    out.resolvedBaseUrl = SafecastProtocol::resolveBaseUrl(
        productionBaseUrl,
        config.safecastUseTestApi,
        config.safecastCustomApiBaseUrl);
    if (!SafecastProtocol::parseBaseUrl(out.resolvedBaseUrl, out.endpoint))
        return config.safecastCustomApiBaseUrl.length() ? Error::InvalidCustomApiBaseUrl : Error::InvalidApiBaseUrl;

    const bool hasLatitude = config.safecastLatitude.length() > 0;
    const bool hasLongitude = config.safecastLongitude.length() > 0;
    if (requirePublishableFields && !hasLatitude)
        return Error::MissingLatitude;
    if (requirePublishableFields && !hasLongitude)
        return Error::MissingLongitude;

    if (hasLatitude)
    {
        if (!parseFloatText(config.safecastLatitude, out.latitude) || out.latitude < -90.0f || out.latitude > 90.0f)
            return Error::InvalidLatitude;
    }

    if (hasLongitude)
    {
        if (!parseFloatText(config.safecastLongitude, out.longitude) || out.longitude < -180.0f || out.longitude > 180.0f)
            return Error::InvalidLongitude;
    }

    if (requirePublishableFields && !out.apiKey.length())
        return Error::MissingApiKey;

    return Error::None;
}
} // namespace SafecastConfig
