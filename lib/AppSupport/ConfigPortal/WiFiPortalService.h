#pragma once

#include <WiFi.h>
#include <WiFiManager.h>
#include "AppConfig/AppConfig.h"

class WiFiPortalService
{
public:
    WiFiPortalService(AppConfig &config, AppConfigStore &store, Print &logPort);

    void begin();
    bool connect(bool forcePortal);
    void maintain();
    void process();
    void syncIfRequested();
    void dumpStatus();
    void enableStatusLogging();

private:
    void refreshParameters();
    void attachParameters();
    bool applyFromParameters(bool persist, bool forceSave = false);
    void handleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    void logStatusIfNeeded();
    void logConnectionDetails(const IPAddress &ip, const IPAddress &gateway, const IPAddress &mask);
    void logStatus();
    void attemptReconnect();

    AppConfig &config_;
    AppConfigStore &store_;
    WiFiManager manager_;
    Print &log_;

    WiFiManagerParameter paramDeviceName_;
    WiFiManagerParameter paramMqttHost_;
    WiFiManagerParameter paramMqttPort_;
    WiFiManagerParameter paramMqttClient_;
    WiFiManagerParameter paramMqttUser_;
    WiFiManagerParameter paramMqttPass_;
    WiFiManagerParameter paramMqttTopic_;
    WiFiManagerParameter paramMqttFullTopic_;
    WiFiManagerParameter paramReadInterval_;
    bool paramsAttached_;
    wl_status_t lastStatus_;
    WiFiEventId_t wifiEventId_;
    IPAddress lastIp_;
    bool hasLoggedIp_;
    bool loggingEnabled_ = false;
    bool pendingReconnect_ = false;
    unsigned long lastReconnectAttemptMs_ = 0;
    String lastKnownSsid_;
    String lastKnownPass_;
};
