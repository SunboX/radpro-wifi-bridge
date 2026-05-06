#pragma once

#include <Arduino.h>

namespace GmcMapPortalLinks
{
inline String trimId(const String &value)
{
    String trimmed = value;
    trimmed.trim();
    return trimmed;
}

inline String buildGmcMapDeviceHistoryUrl(const String &deviceId)
{
    const String trimmedDeviceId = trimId(deviceId);
    if (!trimmedDeviceId.length())
        return String();

    return String("https://www.gmcmap.com/historyData.asp?Param_ID=") + trimmedDeviceId + "&systemTimeZone=1";
}
} // namespace GmcMapPortalLinks
