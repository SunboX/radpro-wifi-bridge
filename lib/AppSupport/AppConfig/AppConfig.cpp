// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppConfig.h"

namespace
{
    constexpr const char *kPrefsNamespace = "radprocfg";
}

AppConfigStore::AppConfigStore() : saveRequested_(false) {}

bool AppConfigStore::load(AppConfig &cfg)
{
    if (!prefs_.begin(kPrefsNamespace, true))
    {
        return false;
    }

    cfg.deviceName = prefs_.getString("devName", cfg.deviceName);
    cfg.deviceName.trim();

    cfg.mqttEnabled = prefs_.getBool("mqttEnabled", cfg.mqttEnabled);

    cfg.mqttHost = prefs_.getString("mqttHost", cfg.mqttHost);
    cfg.mqttHost.trim();

    cfg.mqttPort = prefs_.getUShort("mqttPort", cfg.mqttPort);

    cfg.mqttClient = prefs_.getString("mqttClient", cfg.mqttClient);
    cfg.mqttClient.trim();

    cfg.mqttUser = prefs_.getString("mqttUser", cfg.mqttUser);
    cfg.mqttUser.trim();

    cfg.mqttPassword = prefs_.getString("mqttPass", cfg.mqttPassword);

    cfg.mqttTopic = prefs_.getString("mqttTopic", cfg.mqttTopic);
    cfg.mqttTopic.trim();

    cfg.mqttFullTopic = prefs_.getString("mqttFullTopic", cfg.mqttFullTopic);
    cfg.mqttFullTopic.trim();

    cfg.readIntervalMs = prefs_.getUInt("readInterval", cfg.readIntervalMs);

    cfg.openSenseMapEnabled = prefs_.getBool("osemEnabled", cfg.openSenseMapEnabled);
    cfg.openSenseBoxId = prefs_.getString("osemBoxId", cfg.openSenseBoxId);
    cfg.openSenseBoxId.trim();
    cfg.openSenseApiKey = prefs_.getString("osemApiKey", cfg.openSenseApiKey);
    cfg.openSenseApiKey.trim();
    cfg.openSenseTubeRateSensorId = prefs_.getString("osemRateId", cfg.openSenseTubeRateSensorId);
    cfg.openSenseTubeRateSensorId.trim();
    cfg.openSenseDoseRateSensorId = prefs_.getString("osemDoseId", cfg.openSenseDoseRateSensorId);
    cfg.openSenseDoseRateSensorId.trim();

    cfg.gmcMapEnabled = prefs_.getBool("gmcEnabled", cfg.gmcMapEnabled);
    cfg.gmcMapAccountId = prefs_.getString("gmcAccount", cfg.gmcMapAccountId);
    cfg.gmcMapAccountId.trim();
    cfg.gmcMapDeviceId = prefs_.getString("gmcDevice", cfg.gmcMapDeviceId);
    cfg.gmcMapDeviceId.trim();
    cfg.radmonEnabled = prefs_.getBool("radmonEnabled", cfg.radmonEnabled);
    cfg.radmonUser = prefs_.getString("radmonUser", cfg.radmonUser);
    cfg.radmonUser.trim();
    cfg.radmonPassword = prefs_.getString("radmonPass", cfg.radmonPassword);
    cfg.openRadiationEnabled = prefs_.getBool("orEnabled", cfg.openRadiationEnabled);
    cfg.openRadiationDeviceId = prefs_.getString("orDeviceId", cfg.openRadiationDeviceId);
    cfg.openRadiationDeviceId.trim();
    cfg.openRadiationApiKey = prefs_.getString("orApiKey", cfg.openRadiationApiKey);
    cfg.openRadiationApiKey.trim();
    cfg.openRadiationUserId = prefs_.getString("orUserId", cfg.openRadiationUserId);
    cfg.openRadiationUserId.trim();
    cfg.openRadiationUserPassword = prefs_.getString("orUserPwd", cfg.openRadiationUserPassword);
    cfg.openRadiationMeasurementEnvironment = prefs_.getString("orEnv", cfg.openRadiationMeasurementEnvironment);
    cfg.openRadiationMeasurementEnvironment.trim();
    cfg.openRadiationMeasurementHeight = prefs_.getFloat("orHeight", cfg.openRadiationMeasurementHeight);
    cfg.openRadiationLatitude = prefs_.getFloat("orLat", cfg.openRadiationLatitude);
    cfg.openRadiationLongitude = prefs_.getFloat("orLon", cfg.openRadiationLongitude);
    cfg.openRadiationAltitude = prefs_.getFloat("orAlt", cfg.openRadiationAltitude);
    cfg.openRadiationAccuracy = prefs_.getFloat("orAcc", cfg.openRadiationAccuracy);
    cfg.safecastEnabled = prefs_.getBool("scEnabled", cfg.safecastEnabled);
    cfg.safecastApiBaseUrl = prefs_.getString("scBaseUrl", cfg.safecastApiBaseUrl);
    cfg.safecastApiBaseUrl.trim();
    cfg.safecastUseTestApi = prefs_.getBool("scUseTest", cfg.safecastUseTestApi);
    cfg.safecastCustomApiBaseUrl = prefs_.getString("scCustomUrl", cfg.safecastCustomApiBaseUrl);
    cfg.safecastCustomApiBaseUrl.trim();
    cfg.safecastApiKey = prefs_.getString("scApiKey", cfg.safecastApiKey);
    cfg.safecastApiKey.trim();
    cfg.safecastDeviceId = prefs_.getString("scDeviceId", cfg.safecastDeviceId);
    cfg.safecastDeviceId.trim();
    cfg.safecastLatitude = prefs_.getString("scLat", cfg.safecastLatitude);
    cfg.safecastLatitude.trim();
    cfg.safecastLongitude = prefs_.getString("scLon", cfg.safecastLongitude);
    cfg.safecastLongitude.trim();
    cfg.safecastHeightCm = prefs_.getString("scHeight", cfg.safecastHeightCm);
    cfg.safecastHeightCm.trim();
    cfg.safecastLocationName = prefs_.getString("scLocName", cfg.safecastLocationName);
    cfg.safecastLocationName.trim();
    cfg.safecastUnit = prefs_.getString("scUnit", cfg.safecastUnit);
    cfg.safecastUnit.trim();
    cfg.safecastUploadIntervalSeconds = prefs_.getUInt("scUpInt", cfg.safecastUploadIntervalSeconds);
    cfg.safecastDebug = prefs_.getBool("scDebug", cfg.safecastDebug);

    prefs_.end();

    if (cfg.readIntervalMs < kMinReadIntervalMs)
        cfg.readIntervalMs = kMinReadIntervalMs;
    if (!cfg.safecastApiBaseUrl.length())
        cfg.safecastApiBaseUrl = "https://api.safecast.org";
    if (!cfg.safecastUnit.length())
        cfg.safecastUnit = "cpm";
    if (cfg.safecastUploadIntervalSeconds < kMinSafecastUploadIntervalSeconds)
        cfg.safecastUploadIntervalSeconds = kDefaultSafecastUploadIntervalSeconds;

    return true;
}

bool AppConfigStore::save(const AppConfig &cfg)
{
    if (!prefs_.begin(kPrefsNamespace, false))
    {
        return false;
    }

    prefs_.putString("devName", cfg.deviceName);
    prefs_.putBool("mqttEnabled", cfg.mqttEnabled);
    prefs_.putString("mqttHost", cfg.mqttHost);
    prefs_.putUShort("mqttPort", cfg.mqttPort);
    prefs_.putString("mqttClient", cfg.mqttClient);
    prefs_.putString("mqttUser", cfg.mqttUser);
    prefs_.putString("mqttPass", cfg.mqttPassword);
    prefs_.putString("mqttTopic", cfg.mqttTopic);
    prefs_.putString("mqttFullTopic", cfg.mqttFullTopic);
    prefs_.putUInt("readInterval", cfg.readIntervalMs);
    prefs_.putBool("osemEnabled", cfg.openSenseMapEnabled);
    prefs_.putString("osemBoxId", cfg.openSenseBoxId);
    prefs_.putString("osemApiKey", cfg.openSenseApiKey);
    prefs_.putString("osemRateId", cfg.openSenseTubeRateSensorId);
    prefs_.putString("osemDoseId", cfg.openSenseDoseRateSensorId);
    prefs_.putBool("gmcEnabled", cfg.gmcMapEnabled);
    prefs_.putString("gmcAccount", cfg.gmcMapAccountId);
    prefs_.putString("gmcDevice", cfg.gmcMapDeviceId);
    prefs_.putBool("radmonEnabled", cfg.radmonEnabled);
    prefs_.putString("radmonUser", cfg.radmonUser);
    prefs_.putString("radmonPass", cfg.radmonPassword);
    prefs_.putBool("orEnabled", cfg.openRadiationEnabled);
    prefs_.putString("orDeviceId", cfg.openRadiationDeviceId);
    prefs_.putString("orApiKey", cfg.openRadiationApiKey);
    prefs_.putString("orUserId", cfg.openRadiationUserId);
    prefs_.putString("orUserPwd", cfg.openRadiationUserPassword);
    prefs_.putString("orEnv", cfg.openRadiationMeasurementEnvironment);
    prefs_.putFloat("orHeight", cfg.openRadiationMeasurementHeight);
    prefs_.putFloat("orLat", cfg.openRadiationLatitude);
    prefs_.putFloat("orLon", cfg.openRadiationLongitude);
    prefs_.putFloat("orAlt", cfg.openRadiationAltitude);
    prefs_.putFloat("orAcc", cfg.openRadiationAccuracy);
    prefs_.putBool("scEnabled", cfg.safecastEnabled);
    prefs_.putString("scBaseUrl", cfg.safecastApiBaseUrl);
    prefs_.putBool("scUseTest", cfg.safecastUseTestApi);
    prefs_.putString("scCustomUrl", cfg.safecastCustomApiBaseUrl);
    prefs_.putString("scApiKey", cfg.safecastApiKey);
    prefs_.putString("scDeviceId", cfg.safecastDeviceId);
    prefs_.putString("scLat", cfg.safecastLatitude);
    prefs_.putString("scLon", cfg.safecastLongitude);
    prefs_.putString("scHeight", cfg.safecastHeightCm);
    prefs_.putString("scLocName", cfg.safecastLocationName);
    prefs_.putString("scUnit", cfg.safecastUnit);
    prefs_.putUInt("scUpInt", cfg.safecastUploadIntervalSeconds);
    prefs_.putBool("scDebug", cfg.safecastDebug);

    prefs_.end();
    return true;
}

void AppConfigStore::requestSave()
{
    saveRequested_ = true;
}

bool AppConfigStore::consumeSaveRequest()
{
    if (!saveRequested_)
        return false;
    saveRequested_ = false;
    return true;
}
