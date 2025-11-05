#pragma once

#include <Arduino.h>
#include <Preferences.h>

constexpr uint32_t kMinReadIntervalMs = 500;
constexpr size_t kDeviceNameParamLen = 32;
constexpr size_t kMqttHostParamLen = 64;
constexpr size_t kMqttClientParamLen = 48;
constexpr size_t kMqttUserParamLen = 48;
constexpr size_t kMqttPassParamLen = 64;
constexpr size_t kMqttTopicParamLen = 64;
constexpr size_t kMqttFullTopicParamLen = 64;
constexpr size_t kReadIntervalParamLen = 12;
constexpr size_t kMqttPortParamLen = 6;
constexpr size_t kOsemBoxIdLen = 64;
constexpr size_t kOsemApiKeyLen = 80;
constexpr size_t kOsemSensorIdLen = 64;
constexpr size_t kRadmonUserLen = 48;
constexpr size_t kRadmonPasswordLen = 48;
constexpr size_t kOpenRadiationDeviceIdLen = 64;
constexpr size_t kOpenRadiationApiKeyLen = 96;

struct AppConfig
{
    String deviceName = "RadPro WiFi Bridge";
    bool mqttEnabled = false;
    String mqttHost;
    uint16_t mqttPort = 1883;
    String mqttClient = "radpro-bridge";
    String mqttUser;
    String mqttPassword;
    String mqttTopic = "radpro/%deviceid%";
    String mqttFullTopic = "%prefix%/%topic%/";
    uint32_t readIntervalMs = 1000;
    bool openSenseMapEnabled = false;
    String openSenseBoxId;
    String openSenseApiKey;
    String openSenseTubeRateSensorId;
    String openSenseDoseRateSensorId;
    bool gmcMapEnabled = false;
    String gmcMapAccountId;
    String gmcMapDeviceId;
    bool radmonEnabled = false;
    String radmonUser;
    String radmonPassword;
    bool openRadiationEnabled = false;
    String openRadiationDeviceId;
    String openRadiationApiKey;
    float openRadiationLatitude = 0.0f;
    float openRadiationLongitude = 0.0f;
    float openRadiationAltitude = 0.0f;
    float openRadiationAccuracy = 0.0f;
};

inline bool UpdateStringIfChanged(String &target, const char *value)
{
    String trimmed = value ? String(value) : String();
    trimmed.trim();
    if (trimmed == target)
        return false;
    target = trimmed;
    return true;
}

class AppConfigStore
{
public:
    AppConfigStore();

    bool load(AppConfig &cfg);
    bool save(const AppConfig &cfg);

    void requestSave();
    bool consumeSaveRequest();

private:
    Preferences prefs_;
    volatile bool saveRequested_;
};
