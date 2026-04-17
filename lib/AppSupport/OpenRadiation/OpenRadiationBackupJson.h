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
}

inline void applyMeasurementConfig(JsonVariantConst root, AppConfig &config)
{
    if (!root["openRadiationMeasurementEnvironment"].isNull())
    {
        const char *rawEnvironment = root["openRadiationMeasurementEnvironment"].as<const char *>();
        String environment = rawEnvironment ? String(rawEnvironment) : String();
        environment.trim();
        if (!environment.length())
        {
            config.openRadiationMeasurementEnvironment = String();
        }
        else if (OpenRadiationMeasurementMetadata::isValidMeasurementEnvironment(environment))
        {
            config.openRadiationMeasurementEnvironment = environment;
        }
    }

    if (!root["openRadiationMeasurementHeight"].isNull())
    {
        config.openRadiationMeasurementHeight = OpenRadiationMeasurementMetadata::clampMeasurementHeight(
            root["openRadiationMeasurementHeight"].as<float>());
    }
}
} // namespace OpenRadiationBackupJson
