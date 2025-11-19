#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include <vector>
#include "AppConfig/AppConfig.h"
#include "DeviceManager.h"

class WebServer;
class LedController;
class WiFiPortalService;

class GmcMapPublisher
{
public:
    GmcMapPublisher(AppConfig &config, Print &log, const char *bridgeVersion);

    void begin();
    void updateConfig();
    void loop();
    void onCommandResult(DeviceManager::CommandType type, const String &value);
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
    void addRateSample(float cpm, unsigned long now);
    void pruneSamples(unsigned long now);
    bool computeAcpm(float &out);
    static String formatFloat(float value, uint8_t decimals = 3);

    AppConfig &config_;
    Print &log_;
    String bridgeVersion_;
    String pendingCpm_;
    String pendinguSv_;
    float pendingCpmValue_ = 0.0f;
    bool haveCpm_ = false;
    bool haveuSv_ = false;
    bool publishQueued_ = false;
    unsigned long lastAttemptMs_ = 0;
    unsigned long suppressUntilMs_ = 0;
    struct RateSample
    {
        unsigned long timestamp;
        float cpm;
    };
    std::vector<RateSample> rateSamples_;
    float rateSampleSum_ = 0.0f;
};
