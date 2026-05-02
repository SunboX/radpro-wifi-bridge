#pragma once

#include <Arduino.h>
#include <cmath>

namespace GmcMapPayload
{
inline String formatCpm(float cpm)
{
    if (!std::isfinite(cpm) || cpm <= 0.0f)
        return "0";

    return String(static_cast<long>(std::floor(cpm + 0.5f)));
}

inline String buildLogQuery(const String &accountId,
                            const String &deviceId,
                            float cpm,
                            float acpm,
                            const String &uSv)
{
    String query = "/log2.asp?AID=";
    query += accountId;
    query += "&GID=";
    query += deviceId;
    query += "&CPM=";
    query += formatCpm(cpm);
    query += "&ACPM=";
    query += formatCpm(acpm);
    query += "&uSV=";
    query += uSv;
    return query;
}
} // namespace GmcMapPayload
