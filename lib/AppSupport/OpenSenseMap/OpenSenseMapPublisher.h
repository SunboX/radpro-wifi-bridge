#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "AppConfig/AppConfig.h"
#include "DeviceManager.h"

class WebServer;
class LedController;
class WiFiPortalService;

class OpenSenseMapPublisher
{
public:
    OpenSenseMapPublisher(AppConfig &config, Print &log, const char *bridgeVersion);

    void begin();
    void updateConfig();
    void loop();
    void onCommandResult(DeviceManager::CommandType type, const String &value);
    void setPaused(bool paused) { paused_ = paused; }
    static void SendPortalForm(WiFiPortalService &portal, const String &message = String());
    static void HandlePortalPost(WebServer &server,
                                 AppConfig &config,
                                 AppConfigStore &store,
                                 LedController &led,
                                 Print &log,
                                 String &message);

private:
    bool isEnabled() const;
    bool publishPending();
    bool sendPayload(const JsonDocument &payload);

    AppConfig &config_;
    Print &log_;
    String bridgeVersion_;
    String pendingTubeValue_;
    String pendingDoseValue_;
    bool haveTubeValue_ = false;
    bool haveDoseValue_ = false;
    bool pendingPublish_ = false;
    unsigned long lastAttemptMs_ = 0;
    unsigned long suppressUntilMs_ = 0;
    uint8_t consecutiveFailures_ = 0;
    int lastTlsErrorCode_ = 0;
    String lastTlsErrorText_;
    bool paused_ = false;
    JsonDocument payloadDoc_;
};
