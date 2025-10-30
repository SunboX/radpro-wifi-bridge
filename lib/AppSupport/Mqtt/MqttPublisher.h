#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "AppConfig/AppConfig.h"
#include "DeviceManager.h"

class MqttPublisher
{
public:
    MqttPublisher(AppConfig &config, Print &log);

    void begin();
    void updateConfig();
    void loop();
    void onCommandResult(DeviceManager::CommandType type, const String &value);

private:
    bool ensureConnected();
    void refreshTopics();
    String buildTopic(const String &leaf) const;
    String commandLeaf(DeviceManager::CommandType type) const;
    String makeSlug(const String &raw) const;
    String sanitizedDeviceId() const;
    void publish(const String &leaf, const String &payload, bool retain = true);

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
};
