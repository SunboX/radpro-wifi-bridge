#pragma once

#include <Arduino.h>

namespace GmcMapLogRedaction
{
static constexpr const char *kRedactedSecret = "***REDACTED***";

inline String redactParam(const String &query, const char *paramName)
{
    if (!paramName || !paramName[0])
        return query;

    String markerQuestion = String("?") + paramName + "=";
    String markerAmp = String("&") + paramName + "=";

    int markerStart = query.indexOf(markerQuestion);
    size_t markerLength = markerQuestion.length();
    if (markerStart < 0)
    {
        markerStart = query.indexOf(markerAmp);
        markerLength = markerAmp.length();
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

inline String redactQueryForLogs(const String &query)
{
    String redacted = redactParam(query, "AID");
    redacted = redactParam(redacted, "GID");
    return redacted;
}
} // namespace GmcMapLogRedaction
