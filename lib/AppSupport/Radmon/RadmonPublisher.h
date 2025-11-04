#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include "AppConfig/AppConfig.h"
#include "DeviceManager.h"

class RadmonPublisher
{
public:
    RadmonPublisher(AppConfig &config, Print &log, const char *bridgeVersion);

    void begin();
    void updateConfig();
    void loop();
    void onCommandResult(DeviceManager::CommandType type, const String &value);

private:
    bool isEnabled() const;
    bool publishPending();
    bool sendRequest(const String &query);
    static String urlEncode(const String &input);

    AppConfig &config_;
    Print &log_;
    String bridgeVersion_;
    String pendingCpm_;
    String pendingUsv_;
    bool haveCpm_ = false;
    bool haveUsv_ = false;
    bool publishQueued_ = false;
    unsigned long lastAttemptMs_ = 0;
    unsigned long suppressUntilMs_ = 0;
};
