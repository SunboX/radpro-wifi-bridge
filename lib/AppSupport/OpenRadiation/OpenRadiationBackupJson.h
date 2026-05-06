/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <ArduinoJson.h>

#include "AppConfig/AppConfig.h"
#include "OpenRadiation/OpenRadiationMeasurementMetadata.h"

namespace OpenRadiationBackupJson
{
inline void appendMeasurementConfig(JsonDocument &doc, const AppConfig &config)
{
    doc["openRadiationMeasurementEnvironment"] = config.openRadiationMeasurementEnvironment;
    doc["openRadiationMeasurementHeight"] = config.openRadiationMeasurementHeight;
    doc["openRadiationUserId"] = config.openRadiationUserId;
    doc["openRadiationUserPassword"] = config.openRadiationUserPassword;
}

inline void applyMeasurementConfig(JsonVariantConst root, AppConfig &config)
{
    if (!root["openRadiationMeasurementEnvironment"].isNull())
    {
        const char *rawEnvironment = root["openRadiationMeasurementEnvironment"].as<const char *>();
        String environment = rawEnvironment ? String(rawEnvironment) : String();
        environment.trim();
        if (OpenRadiationMeasurementMetadata::isValidMeasurementEnvironment(environment))
        {
            config.openRadiationMeasurementEnvironment = environment;
        }
    }

    if (!root["openRadiationMeasurementHeight"].isNull())
    {
        config.openRadiationMeasurementHeight = OpenRadiationMeasurementMetadata::clampMeasurementHeight(
            root["openRadiationMeasurementHeight"].as<float>());
    }

    if (!root["openRadiationUserId"].isNull())
    {
        const char *rawUserId = root["openRadiationUserId"].as<const char *>();
        String userId = rawUserId ? String(rawUserId) : String();
        userId.trim();
        config.openRadiationUserId = userId;
    }

    if (!root["openRadiationUserPassword"].isNull())
    {
        const char *rawUserPassword = root["openRadiationUserPassword"].as<const char *>();
        config.openRadiationUserPassword = rawUserPassword ? String(rawUserPassword) : String();
    }
}
} // namespace OpenRadiationBackupJson
