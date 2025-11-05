#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "AppConfig/AppConfig.h"
#include "DeviceManager.h"

class OpenRadiationPublisher
{
public:
    OpenRadiationPublisher(AppConfig &config, Print &log, const char *bridgeVersion);

    void begin();
    void updateConfig();
    void loop();
    void onCommandResult(DeviceManager::CommandType type, const String &value);

private:
    bool isEnabled() const;
    bool publishPending();
    bool sendPayload(const String &payload);
    bool buildPayload(String &outJson, float doseRate, int hitCount, String &timestamp);
    bool makeIsoTimestamp(String &out) const;
    static String generateUuid();

    AppConfig &config_;
    Print &log_;
    String bridgeVersion_;
    String pendingDoseValue_;
    String pendingTubeValue_;
    bool haveDoseValue_ = false;
    bool haveTubeValue_ = false;
    bool publishQueued_ = false;
    unsigned long lastAttemptMs_ = 0;
    unsigned long suppressUntilMs_ = 0;
};
