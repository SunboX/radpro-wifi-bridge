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

    prefs_.end();

    if (cfg.readIntervalMs < kMinReadIntervalMs)
        cfg.readIntervalMs = kMinReadIntervalMs;

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
