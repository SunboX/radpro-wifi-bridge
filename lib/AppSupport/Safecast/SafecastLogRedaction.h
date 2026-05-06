#pragma once

#include <Arduino.h>

namespace SafecastLogRedaction
{
static constexpr const char *kRedactedSecret = "***REDACTED***";

inline String redactParam(const String &url, const char *paramName)
{
    if (!paramName || !paramName[0])
        return url;

    String markerQuestion = String("?") + paramName + "=";
    String markerAmp = String("&") + paramName + "=";

    int markerStart = url.indexOf(markerQuestion);
    size_t markerLength = markerQuestion.length();
    if (markerStart < 0)
    {
        markerStart = url.indexOf(markerAmp);
        markerLength = markerAmp.length();
        if (markerStart < 0)
            return url;
    }

    const size_t valueStart = static_cast<size_t>(markerStart) + markerLength;
    int nextParam = url.indexOf('&', valueStart);
    const size_t valueEnd = nextParam >= 0 ? static_cast<size_t>(nextParam) : url.length();

    String redacted = url.substring(0, valueStart);
    redacted += kRedactedSecret;
    redacted += url.substring(valueEnd);
    return redacted;
}

inline String redactUrlForLogs(const String &url)
{
    return redactParam(url, "api_key");
}

inline String maskSecretForDisplay(const String &secret)
{
    if (!secret.length())
        return String();
    if (secret.length() < 8)
        return String("Configured");

    String masked = secret.substring(0, 4);
    const size_t starCount = secret.length() > 16 ? secret.length() - 8 : 8;
    for (size_t i = 0; i < starCount; ++i)
        masked += '*';
    masked += secret.substring(secret.length() - 4);
    return masked;
}
} // namespace SafecastLogRedaction
