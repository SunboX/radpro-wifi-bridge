#pragma once

#include <WiFi.h>
#include <WiFiManager.h>
#include "AppConfig/AppConfig.h"
#include "Led/LedController.h"

class WiFiPortalService
{
public:
    WiFiPortalService(AppConfig &config, AppConfigStore &store, Print &logPort, LedController &led);

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
    void sendMqttForm(const String &message = String());
    void sendOpenSenseForm(const String &message = String());
    void sendRadmonForm(const String &message = String());
    void sendGmcMapForm(const String &message = String());
    void sendOpenRadiationForm(const String &message = String());
    void handleMqttPost();
    void handleOpenSensePost();
    void handleRadmonPost();
    void handleGmcMapPost();
    void handleOpenRadiationPost();
    static String htmlEscape(const String &value);

    AppConfig &config_;
    AppConfigStore &store_;
    WiFiManager manager_;
    Print &log_;
    LedController &led_;

    WiFiManagerParameter paramDeviceName_;
    WiFiManagerParameter paramMqttHost_;
    WiFiManagerParameter paramMqttPort_;
    WiFiManagerParameter paramMqttClient_;
    WiFiManagerParameter paramMqttUser_;
    WiFiManagerParameter paramMqttPass_;
    WiFiManagerParameter paramMqttTopic_;
    WiFiManagerParameter paramMqttFullTopic_;
    WiFiManagerParameter paramReadInterval_;
    WiFiManagerParameter paramGmcAccount_;
    WiFiManagerParameter paramGmcDevice_;
    WiFiManagerParameter paramRadmonUser_;
    WiFiManagerParameter paramRadmonPassword_;
    WiFiManagerParameter paramOpenRadiationDevice_;
    WiFiManagerParameter paramOpenRadiationApiKey_;
    bool paramsAttached_;
    wl_status_t lastStatus_;
    WiFiEventId_t wifiEventId_;
    IPAddress lastIp_;
    bool hasLoggedIp_;
    bool loggingEnabled_ = false;
    bool pendingReconnect_ = false;
    unsigned long lastReconnectAttemptMs_ = 0;
    unsigned long waitingForIpSinceMs_ = 0;
    String lastKnownSsid_;
    String lastKnownPass_;
};
