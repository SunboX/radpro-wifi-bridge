#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <array>
#include <functional>
#include "AppConfig/AppConfig.h"
#include "DeviceManager.h"
#include "Led/LedController.h"

class MqttPublisher
{
public:
    MqttPublisher(AppConfig &config, Print &log, LedController &led);

    void begin();
    void updateConfig();
    void loop();
    void onCommandResult(DeviceManager::CommandType type, const String &value);
    void setPublishCallback(std::function<void(bool)> cb) { publishCallback_ = std::move(cb); }
    void setBridgeVersion(const String &version);

private:
    bool ensureConnected();
    void refreshTopics();
    void publishDiscovery();
    bool publishDiscoveryEntity(DeviceManager::CommandType type,
                                const char *component,
                                const char *objectId,
                                const char *name,
                                const char *unit,
                                const char *deviceClass,
                                const char *stateClass,
                                const char *payloadOn,
                                const char *payloadOff,
                                const char *entityCategory = nullptr,
                                const char *leafOverride = nullptr);
    String buildTopic(const String &leaf) const;
    String commandLeaf(DeviceManager::CommandType type) const;
    String makeSlug(const String &raw) const;
    String sanitizedDeviceId() const;
    bool publish(const String &leaf, const String &payload, bool retain = true);
    bool publishCommand(DeviceManager::CommandType type, const String &payload, bool retain = true);
    String deviceNameForDiscovery() const;
    String deviceModelForDiscovery() const;
    void markAllPending();
    void republishRetained();
    bool publishVersionDiscovery();
    bool publishBridgeVersion();
    struct RetainedState
    {
        DeviceManager::CommandType type;
        String payload;
        bool hasValue = false;
        bool pending = false;
    };
    RetainedState *retainedEntry(DeviceManager::CommandType type);

    AppConfig &config_;
    Print &log_;
    WiFiClient wifi_client_;
    PubSubClient mqtt_client_;

    String currentHost_;
    uint16_t currentPort_ = 1883;
    String currentUser_;
    String currentPassword_;
    String clientIdBase_;
    String topicTemplate_;
    String fullTopicTemplate_;
    bool configValid_ = false;
    bool topicDirty_ = true;
    unsigned long lastReconnectAttempt_ = 0;
    unsigned long lastPublishWarning_ = 0;
    String deviceId_;
    String deviceSlug_;
    String fallbackId_;
    String topicBase_;
    String fullTopicPattern_;
    std::function<void(bool)> publishCallback_;
    String currentDeviceName_;
    String deviceModel_;
    String deviceFirmware_;
    String deviceLocale_;
    bool discoveryPublished_ = false;
    unsigned long lastDiscoveryAttempt_ = 0;
    unsigned long lastRepublishAttempt_ = 0;
    size_t discoveryIndex_ = 0;
    static const std::array<DeviceManager::CommandType, 15> kRetainedTypes_;
    std::array<RetainedState, kRetainedTypes_.size()> retainedStates_;
    LedController &led_;
    String bridgeVersion_;
    bool bridgeVersionDirty_ = true;
    bool versionDiscoveryDone_ = false;
};
