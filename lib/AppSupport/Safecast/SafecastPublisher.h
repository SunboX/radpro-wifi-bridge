#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <vector>

#include "AppConfig/AppConfig.h"
#include "DeviceManager.h"
#include "Publishing/PublisherHealth.h"
#include "Safecast/SafecastConfig.h"

class SafecastPublisher
{
public:
    struct UploadResult
    {
        bool attempted = false;
        bool success = false;
        int statusCode = 0;
        String statusLine;
        String responseBody;
        String errorMessage;
        String redactedUrl;
    };

    SafecastPublisher(AppConfig &config, Print &log, const char *bridgeVersion, PublisherHealth &health);

    void begin();
    void updateConfig();
    void loop();
    void onCommandResult(DeviceManager::CommandType type, const String &value);
    void clearPendingData();
    void setPaused(bool paused) { paused_ = paused; }

    UploadResult sendTestUpload(const AppConfig &configOverride);

private:
    struct RateSample
    {
        unsigned long timestampMs = 0;
        float value = 0.0f;
    };

    struct SampleWindow
    {
        std::vector<RateSample> samples;
        float sum = 0.0f;
        bool hasLatest = false;
        float latestValue = 0.0f;
    };

    bool isEnabled() const;
    bool publishPending();
    void syncHealthState();
    bool makeIsoTimestamp(String &out) const;
    void addSample(SampleWindow &window, float value, unsigned long now);
    void pruneSamples(SampleWindow &window, unsigned long now, unsigned long windowMs);
    bool buildMeasurementFromAverage(const SafecastConfig::ResolvedConfig &resolved,
                                     SafecastPayload::Measurement &outMeasurement,
                                     String &outError);
    bool buildMeasurementFromLatest(const SafecastConfig::ResolvedConfig &resolved,
                                    SafecastPayload::Measurement &outMeasurement,
                                    String &outError);
    UploadResult uploadMeasurement(const SafecastConfig::ResolvedConfig &resolved,
                                   const SafecastPayload::Measurement &measurement,
                                   bool updateHealthState);

    template <typename Client>
    UploadResult sendRequest(Client &client,
                             const SafecastConfig::ResolvedConfig &resolved,
                             const String &payload,
                             bool updateHealthState);

    AppConfig &config_;
    Print &log_;
    String bridgeVersion_;
    PublisherHealth &health_;
    String lastConfigError_;
    SampleWindow cpmWindow_;
    SampleWindow doseWindow_;
    unsigned long lastAttemptMs_ = 0;
    unsigned long suppressUntilMs_ = 0;
    bool paused_ = false;
};
