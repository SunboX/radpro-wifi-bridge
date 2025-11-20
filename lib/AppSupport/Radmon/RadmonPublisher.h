#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include "AppConfig/AppConfig.h"
#include "DeviceManager.h"

class WebServer;
class WiFiPortalService;
class LedController;

class RadmonPublisher
{
public:
    RadmonPublisher(AppConfig &config, Print &log, const char *bridgeVersion);

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
    bool paused_ = false;
};
