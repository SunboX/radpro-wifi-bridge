#pragma once

#include <Arduino.h>

namespace RadmonLogRedaction
{
static constexpr const char *kRedactedSecret = "***REDACTED***";

inline String redactQueryForLogs(const String &query)
{
    int markerStart = query.indexOf("?password=");
    size_t markerLength = 10;
    if (markerStart < 0)
    {
        markerStart = query.indexOf("&password=");
        if (markerStart < 0)
            return query;
    }

    const size_t valueStart = static_cast<size_t>(markerStart) + markerLength;
    int nextParam = query.indexOf('&', valueStart);
    const size_t valueEnd = nextParam >= 0 ? static_cast<size_t>(nextParam) : query.length();

    String redacted = query.substring(0, valueStart);
    redacted += kRedactedSecret;
    redacted += query.substring(valueEnd);
    return redacted;
}
} // namespace RadmonLogRedaction
