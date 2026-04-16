#pragma once

#include <Arduino.h>

namespace OpenSenseMapPortalLinks
{
inline String trimId(const String &value)
{
    String trimmed = value;
    trimmed.trim();
    return trimmed;
}

inline String buildOpenSenseMapBoxUrl(const String &boxId)
{
    const String trimmedBoxId = trimId(boxId);
    if (!trimmedBoxId.length())
        return String();

    return String("https://opensensemap.org/explore/") + trimmedBoxId;
}

inline String buildOpenSenseMapSensorSettingsUrl(const String &boxId, const String &sensorId)
{
    const String trimmedBoxId = trimId(boxId);
    const String trimmedSensorId = trimId(sensorId);

    if (!trimmedBoxId.length() || !trimmedSensorId.length())
        return String();

    return String("https://opensensemap.org/account/") + trimmedBoxId + "/edit/sensors";
}
} // namespace OpenSenseMapPortalLinks
