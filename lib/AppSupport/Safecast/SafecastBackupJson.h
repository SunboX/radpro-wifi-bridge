#pragma once

#include <ArduinoJson.h>

#include "AppConfig/AppConfig.h"
#include "Safecast/SafecastConfig.h"
#include "Safecast/SafecastPayload.h"
#include "Safecast/SafecastProtocol.h"

namespace SafecastBackupJson
{
inline void appendConfig(JsonDocument &doc, const AppConfig &config)
{
    doc["safecastEnabled"] = config.safecastEnabled;
    doc["safecastApiBaseUrl"] = config.safecastApiBaseUrl;
    doc["safecastUseTestApi"] = config.safecastUseTestApi;
    doc["safecastCustomApiBaseUrl"] = config.safecastCustomApiBaseUrl;
    doc["safecastApiKey"] = config.safecastApiKey;
    doc["safecastDeviceId"] = config.safecastDeviceId;
    doc["safecastLatitude"] = config.safecastLatitude;
    doc["safecastLongitude"] = config.safecastLongitude;
    doc["safecastHeightCm"] = config.safecastHeightCm;
    doc["safecastLocationName"] = config.safecastLocationName;
    doc["safecastUnit"] = config.safecastUnit;
    doc["safecastUploadIntervalSeconds"] = config.safecastUploadIntervalSeconds;
    doc["safecastDebug"] = config.safecastDebug;
}

inline void applyConfig(JsonVariantConst input, AppConfig &config)
{
    auto setString = [](String &target, JsonVariantConst value, bool normalizeUrl = false) {
        if (value.isNull())
            return;

        String text;
        if (value.is<const char *>())
            text = String(value.as<const char *>());
        else
            return;

        text.trim();
        if (normalizeUrl && text.length())
            text = SafecastProtocol::normalizeBaseUrl(text);
        target = text;
    };

    auto setBool = [](bool &target, JsonVariantConst value) {
        if (!value.isNull())
            target = value.as<bool>();
    };

    if (!input["safecastEnabled"].isNull())
        config.safecastEnabled = input["safecastEnabled"].as<bool>();
    setString(config.safecastApiBaseUrl, input["safecastApiBaseUrl"], true);
    setBool(config.safecastUseTestApi, input["safecastUseTestApi"]);
    setString(config.safecastCustomApiBaseUrl, input["safecastCustomApiBaseUrl"], true);
    setString(config.safecastApiKey, input["safecastApiKey"]);
    setString(config.safecastDeviceId, input["safecastDeviceId"]);
    setString(config.safecastLatitude, input["safecastLatitude"]);
    setString(config.safecastLongitude, input["safecastLongitude"]);
    setString(config.safecastHeightCm, input["safecastHeightCm"]);
    setString(config.safecastLocationName, input["safecastLocationName"]);

    String unit;
    if (input["safecastUnit"].is<const char *>())
        unit = SafecastPayload::normalizeUnit(String(input["safecastUnit"].as<const char *>()));
    if (unit.length())
        config.safecastUnit = unit;

    if (!input["safecastUploadIntervalSeconds"].isNull())
    {
        uint32_t interval = input["safecastUploadIntervalSeconds"].as<uint32_t>();
        if (interval < SafecastConfig::kMinUploadIntervalSeconds)
            interval = SafecastConfig::kMinUploadIntervalSeconds;
        config.safecastUploadIntervalSeconds = interval;
    }

    setBool(config.safecastDebug, input["safecastDebug"]);

    if (!config.safecastApiBaseUrl.length())
        config.safecastApiBaseUrl = SafecastProtocol::kProductionApiBaseUrl;
    if (!config.safecastUnit.length())
        config.safecastUnit = "cpm";
    if (config.safecastUploadIntervalSeconds < SafecastConfig::kMinUploadIntervalSeconds)
        config.safecastUploadIntervalSeconds = SafecastConfig::kMinUploadIntervalSeconds;
}
} // namespace SafecastBackupJson
