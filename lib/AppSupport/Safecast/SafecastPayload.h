#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cmath>
#include <cstdlib>

namespace SafecastPayload
{
enum class BuildError
{
    None,
    MissingLatitude,
    MissingLongitude,
    MissingValue,
    InvalidValue,
    MissingCapturedAt,
    InvalidUnit,
    InvalidDeviceId,
    InvalidHeight,
};

struct Measurement
{
    bool hasLatitude = false;
    bool hasLongitude = false;
    bool hasValue = false;
    float latitude = 0.0f;
    float longitude = 0.0f;
    float value = 0.0f;
    String unit;
    String capturedAt;
    String deviceId;
    String heightCm;
    String locationName;
};

inline String normalizeUnit(const String &unit)
{
    String trimmed = unit;
    trimmed.trim();
    if (trimmed.equalsIgnoreCase("cpm"))
        return String("cpm");
    if (trimmed.equalsIgnoreCase("usv"))
        return String("usv");
    return String();
}

inline bool isSupportedUnit(const String &unit)
{
    return normalizeUnit(unit).length() > 0;
}

inline bool parseWholeNumber(const String &text, long &out)
{
    String trimmed = text;
    trimmed.trim();
    if (!trimmed.length())
        return false;

    char *end = nullptr;
    const long parsed = std::strtol(trimmed.c_str(), &end, 10);
    if (!end || end == trimmed.c_str() || *end != '\0')
        return false;
    out = parsed;
    return true;
}

inline const char *buildErrorText(BuildError error)
{
    switch (error)
    {
    case BuildError::None:
        return "";
    case BuildError::MissingLatitude:
        return "missing latitude";
    case BuildError::MissingLongitude:
        return "missing longitude";
    case BuildError::MissingValue:
        return "missing measurement value";
    case BuildError::InvalidValue:
        return "invalid measurement value";
    case BuildError::MissingCapturedAt:
        return "missing capture timestamp";
    case BuildError::InvalidUnit:
        return "invalid Safecast unit";
    case BuildError::InvalidDeviceId:
        return "invalid Safecast device ID";
    case BuildError::InvalidHeight:
        return "invalid Safecast height";
    }
    return "unknown Safecast payload error";
}

inline BuildError buildPayloadDocument(JsonDocument &doc, const Measurement &measurement)
{
    if (!measurement.hasLatitude)
        return BuildError::MissingLatitude;
    if (!measurement.hasLongitude)
        return BuildError::MissingLongitude;
    if (!measurement.hasValue)
        return BuildError::MissingValue;
    if (!std::isfinite(measurement.value) || measurement.value < 0.0f)
        return BuildError::InvalidValue;

    const String unit = normalizeUnit(measurement.unit);
    if (!unit.length())
        return BuildError::InvalidUnit;

    String capturedAt = measurement.capturedAt;
    capturedAt.trim();
    if (!capturedAt.length())
        return BuildError::MissingCapturedAt;

    doc.clear();
    doc["latitude"] = measurement.latitude;
    doc["longitude"] = measurement.longitude;
    doc["value"] = measurement.value;
    doc["unit"] = unit;
    doc["captured_at"] = capturedAt;

    if (measurement.deviceId.length())
    {
        long deviceId = 0;
        if (!parseWholeNumber(measurement.deviceId, deviceId) || deviceId < 0)
            return BuildError::InvalidDeviceId;
        doc["device_id"] = deviceId;
    }

    if (measurement.heightCm.length())
    {
        long heightCm = 0;
        if (!parseWholeNumber(measurement.heightCm, heightCm) || heightCm < 0)
            return BuildError::InvalidHeight;
        doc["height"] = heightCm;
    }

    String locationName = measurement.locationName;
    locationName.trim();
    if (locationName.length())
        doc["location_name"] = locationName;

    return BuildError::None;
}
} // namespace SafecastPayload
