/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <Arduino.h>

#include <cmath>
#include <cstdio>

namespace OpenRadiationPortalLinks
{
inline bool hasUsableCoordinates(float latitude, float longitude)
{
    if (latitude < -90.0f || latitude > 90.0f)
        return false;
    if (longitude < -180.0f || longitude > 180.0f)
        return false;

    return !(std::fabs(latitude) < 0.000001f && std::fabs(longitude) < 0.000001f);
}

inline String buildOpenRadiationMapUrl(float latitude, float longitude)
{
    if (!hasUsableCoordinates(latitude, longitude))
        return String();

    char latBuffer[24];
    char lonBuffer[24];
    std::snprintf(latBuffer, sizeof(latBuffer), "%.6f", static_cast<double>(latitude));
    std::snprintf(lonBuffer, sizeof(lonBuffer), "%.6f", static_cast<double>(longitude));

    return String("https://request.openradiation.net/openradiation/14/") + latBuffer + "/" + lonBuffer;
}
} // namespace OpenRadiationPortalLinks
