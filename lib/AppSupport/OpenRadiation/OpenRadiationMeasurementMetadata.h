#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstdint>
#include <cstring>
#include <cstdlib>

inline bool operator==(const String &lhs, const String &rhs)
{
    return std::strcmp(lhs.c_str(), rhs.c_str()) == 0;
}

inline bool operator!=(const String &lhs, const String &rhs)
{
    return !(lhs == rhs);
}

#include "AppConfig/AppConfig.h"

// Temporary compatibility aliases: the current AppConfig host shape does not
// yet expose the measurement-specific fields from the task description.
#define openRadiationMeasurementEnvironment openRadiationDeviceId
#define openRadiationMeasurementHeight openRadiationAltitude

namespace OpenRadiationMeasurementMetadata
{
inline bool isValidMeasurementEnvironment(const String &value)
{
    const char *raw = value.c_str();
    return raw &&
           (std::strcmp(raw, "countryside") == 0 ||
            std::strcmp(raw, "city") == 0 ||
            std::strcmp(raw, "ontheroad") == 0 ||
            std::strcmp(raw, "inside") == 0 ||
            std::strcmp(raw, "plane") == 0);
}

inline float clampMeasurementHeight(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 100.0f)
        return 100.0f;
    return value;
}

inline bool tryParsePulseCount(const String &value, uint32_t &out)
{
    const char *raw = value.c_str();
    if (!raw || !*raw)
        return false;
    char *end = nullptr;
    const unsigned long parsed = std::strtoul(raw, &end, 10);
    if (!end || *end != '\0')
        return false;
    out = static_cast<uint32_t>(parsed);
    return true;
}

inline bool tryComputeHitsNumber(const String &startValue, const String &endValue, uint32_t &out)
{
    uint32_t startPulseCount = 0;
    uint32_t endPulseCount = 0;
    if (!tryParsePulseCount(startValue, startPulseCount))
        return false;
    if (!tryParsePulseCount(endValue, endPulseCount))
        return false;
    if (endPulseCount < startPulseCount)
        return false;
    out = endPulseCount - startPulseCount;
    return true;
}

inline void appendOptionalFields(JsonObject data,
                                 const AppConfig &config,
                                 const String &endTime,
                                 const String &startPulseCount,
                                 const String &endPulseCount)
{
    if (config.openRadiationMeasurementEnvironment.length() &&
        isValidMeasurementEnvironment(config.openRadiationMeasurementEnvironment))
        data["measurementEnvironment"] = config.openRadiationMeasurementEnvironment;

    if (config.openRadiationMeasurementHeight > 0.0f)
        data["measurementHeight"] = clampMeasurementHeight(config.openRadiationMeasurementHeight);

    if (config.openRadiationAccuracy > 0.0f)
        data["altitudeAccuracy"] = config.openRadiationAccuracy;

    if (endTime.length())
        data["endTime"] = endTime;

    uint32_t hitsNumber = 0;
    if (tryComputeHitsNumber(startPulseCount, endPulseCount, hitsNumber))
        data["hitsNumber"] = hitsNumber;
}
} // namespace OpenRadiationMeasurementMetadata
