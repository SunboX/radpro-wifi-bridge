#include "MqttPublisher.h"

const std::array<DeviceManager::CommandType, 15> MqttPublisher::kRetainedTypes_ = {
    DeviceManager::CommandType::DeviceId,
    DeviceManager::CommandType::DevicePower,
    DeviceManager::CommandType::DeviceBatteryVoltage,
    DeviceManager::CommandType::DeviceBatteryPercent,
    DeviceManager::CommandType::DeviceTime,
    DeviceManager::CommandType::DeviceTimeZone,
    DeviceManager::CommandType::DeviceSensitivity,
    DeviceManager::CommandType::TubeTime,
    DeviceManager::CommandType::TubePulseCount,
    DeviceManager::CommandType::TubeRate,
    DeviceManager::CommandType::TubeDoseRate,
    DeviceManager::CommandType::TubeDeadTime,
    DeviceManager::CommandType::TubeDeadTimeCompensation,
    DeviceManager::CommandType::TubeHVFrequency,
    DeviceManager::CommandType::TubeHVDutyCycle};

#include <ctype.h>

MqttPublisher::MqttPublisher(AppConfig &config, Print &log, LedController &led)
    : config_(config),
      log_(log),
      mqtt_client_(wifi_client_),
      led_(led)
{
    uint64_t mac = ESP.getEfuseMac();
    char buf[17];
    snprintf(buf, sizeof(buf), "%012llx", static_cast<unsigned long long>(mac));
    fallbackId_ = String("esp32s3-") + buf;

    for (size_t i = 0; i < retainedStates_.size(); ++i)
    {
        retainedStates_[i].type = kRetainedTypes_[i];
    }
}

void MqttPublisher::begin()
{
    // Discovery payloads can get fairly large; use a bigger MQTT buffer.
    mqtt_client_.setBufferSize(1024);
    updateConfig();
}

void MqttPublisher::updateConfig()
{
    String host = config_.mqttHost;
    host.trim();
    uint16_t port = config_.mqttPort ? config_.mqttPort : 1883;

    if (!config_.mqttEnabled || host.isEmpty())
    {
        if (mqtt_client_.connected())
            mqtt_client_.disconnect();
        if (configValid_)
        {
            log_.println("MQTT disabled; publisher idle.");
            configValid_ = false;
        }
        currentHost_ = host;
        currentPort_ = port;
        currentUser_ = config_.mqttUser;
        currentPassword_ = config_.mqttPassword;
        clientIdBase_ = config_.mqttClient;
        topicTemplate_ = config_.mqttTopic;
        fullTopicTemplate_ = config_.mqttFullTopic;
        return;
    }

    if (currentDeviceName_ != config_.deviceName)
    {
        currentDeviceName_ = config_.deviceName;
        discoveryPublished_ = false;
        versionDiscoveryDone_ = false;
    }

    if (host == currentHost_ && port == currentPort_ &&
        config_.mqttUser == currentUser_ &&
        config_.mqttPassword == currentPassword_ &&
        config_.mqttClient == clientIdBase_ &&
        config_.mqttTopic == topicTemplate_ &&
        config_.mqttFullTopic == fullTopicTemplate_)
    {
        return;
    }

    currentHost_ = host;
    currentPort_ = port;
    currentUser_ = config_.mqttUser;
    currentPassword_ = config_.mqttPassword;
    clientIdBase_ = config_.mqttClient;
    topicTemplate_ = config_.mqttTopic;
    fullTopicTemplate_ = config_.mqttFullTopic;

    topicDirty_ = true;
    discoveryPublished_ = false;
    versionDiscoveryDone_ = false;
    bridgeVersionDirty_ = true;
    lastDiscoveryAttempt_ = 0;
    markAllPending();
    discoveryIndex_ = 0;

    if (currentHost_.length())
    {
        log_.print("MQTT config updated: host=");
        log_.print(currentHost_);
        log_.print(" port=");
        log_.println(currentPort_);
    }
    else
    {
        log_.println("MQTT config updated: host empty, MQTT disabled.");
    }

    if (mqtt_client_.connected())
    {
        mqtt_client_.disconnect();
    }

    if (currentHost_.length())
    {
        mqtt_client_.setServer(currentHost_.c_str(), currentPort_);
        configValid_ = true;
        lastReconnectAttempt_ = 0;
    }
    else
    {
        configValid_ = false;
    }
}

void MqttPublisher::loop()
{
    if (!config_.mqttEnabled)
        return;

    if (!configValid_)
        return;

    if (!mqtt_client_.connected())
    {
        ensureConnected();
    }

    if (mqtt_client_.connected())
    {
        mqtt_client_.loop();
        publishDiscovery();
        republishRetained();
    }
}

void MqttPublisher::onCommandResult(DeviceManager::CommandType type, const String &value)
{
    if (!config_.mqttEnabled)
        return;

    switch (type)
    {
    case DeviceManager::CommandType::DeviceModel:
        if (value != deviceModel_)
        {
            deviceModel_ = value;
            discoveryPublished_ = false;
        }
        return;
    case DeviceManager::CommandType::DeviceFirmware:
        if (value != deviceFirmware_)
        {
            deviceFirmware_ = value;
            discoveryPublished_ = false;
        }
        return;
    case DeviceManager::CommandType::DeviceLocale:
        deviceLocale_ = value;
        return;
    default:
        break;
    }

    if (type == DeviceManager::CommandType::DeviceId)
    {
        if (!value.length())
            return;
        deviceId_ = value;
        deviceSlug_ = makeSlug(value);
        topicDirty_ = true;
        discoveryPublished_ = false;
        markAllPending();
        discoveryIndex_ = 0;
        publishCommand(type, value, true);
        return;
    }

    if (type == DeviceManager::CommandType::DevicePower)
    {
        String payload = (value == "1" ? "ON" : (value == "0" ? "OFF" : value));
        publishCommand(type, payload, true);
        return;
    }

    if (type == DeviceManager::CommandType::DeviceBatteryPercent)
    {
        publishCommand(type, value, true);
        return;
    }

    bool retain = true;
    if (type == DeviceManager::CommandType::RandomData || type == DeviceManager::CommandType::DataLog)
        retain = false;
    publishCommand(type, value, retain);
}

bool MqttPublisher::ensureConnected()
{
    if (!configValid_ || currentHost_.isEmpty())
        return false;

    if (mqtt_client_.connected())
    {
        led_.clearFault(FaultCode::MqttUnreachable);
        led_.clearFault(FaultCode::MqttAuthFailure);
        led_.clearFault(FaultCode::MqttConnectionReset);
        return true;
    }

    if (WiFi.status() != WL_CONNECTED)
        return false;

    unsigned long now = millis();
    if (now - lastReconnectAttempt_ < 5000)
        return false;

    lastReconnectAttempt_ = now;

    String clientId = clientIdBase_.length() ? clientIdBase_ : String("radpro-bridge");
    String slug = sanitizedDeviceId();
    if (slug.length())
    {
        if (!clientId.endsWith(slug))
        {
            clientId += "-";
            clientId += slug;
        }
    }

    bool connected = false;
    if (currentUser_.length())
    {
        connected = mqtt_client_.connect(clientId.c_str(), currentUser_.c_str(), currentPassword_.c_str());
    }
    else
    {
        connected = mqtt_client_.connect(clientId.c_str());
    }

    int state = mqtt_client_.state();

    if (connected)
    {
        log_.println("MQTT connected.");
        discoveryPublished_ = false;
        lastDiscoveryAttempt_ = 0;
        markAllPending();
        versionDiscoveryDone_ = false;
        bridgeVersionDirty_ = true;
        republishRetained();
        discoveryIndex_ = 0;
        led_.clearFault(FaultCode::MqttUnreachable);
        led_.clearFault(FaultCode::MqttAuthFailure);
        led_.clearFault(FaultCode::MqttConnectionReset);
    }
    else
    {
        log_.print("MQTT connect failed: ");
        log_.println(state);
        if (state == 5)
        {
            led_.activateFault(FaultCode::MqttAuthFailure);
        }
        else if (state == -2 || state == -4)
        {
            led_.activateFault(FaultCode::MqttUnreachable);
        }
        else if (state == -3 || state == -1)
        {
            led_.activateFault(FaultCode::MqttConnectionReset);
        }
    }

    return connected;
}

void MqttPublisher::refreshTopics()
{
    String slug = sanitizedDeviceId();

    String base = topicTemplate_.length() ? topicTemplate_ : String("radpro/%deviceid%");
    base.trim();
    if (!base.length())
        base = "radpro/%deviceid%";
    base.replace("%deviceid%", slug);
    base.replace("%DeviceId%", slug);
    while (base.endsWith("/"))
        base.remove(base.length() - 1);
    topicBase_ = base;

    fullTopicPattern_ = fullTopicTemplate_.length() ? fullTopicTemplate_ : String("%prefix%/%topic%/");
    if (!fullTopicPattern_.length())
        fullTopicPattern_ = "%prefix%/%topic%/";

    topicDirty_ = false;
}

bool MqttPublisher::publishCommand(DeviceManager::CommandType type, const String &payload, bool retain)
{
    String leaf = commandLeaf(type);
    if (!leaf.length())
        return false;

    RetainedState *entry = retain ? retainedEntry(type) : nullptr;
    if (entry)
    {
        entry->payload = payload;
        entry->hasValue = true;
        entry->pending = true;
    }

    bool ok = publish(leaf, payload, retain);
    if (entry && ok)
        entry->pending = false;
    else if (entry && !ok)
        lastRepublishAttempt_ = 0;
    return ok;
}

String MqttPublisher::buildTopic(const String &leaf) const
{
    String pattern = fullTopicPattern_.length() ? fullTopicPattern_ : String("%prefix%/%topic%/");
    String topic = topicBase_.length() ? topicBase_ : "radpro/" + sanitizedDeviceId();

    String result = pattern;
    result.replace("%prefix%", "stat");
    result.replace("%topic%", topic);
    if (!result.endsWith("/"))
        result += '/';

    String out = result;
    out += leaf;
    return out;
}

String MqttPublisher::commandLeaf(DeviceManager::CommandType type) const
{
    switch (type)
    {
    case DeviceManager::CommandType::DeviceId:
        return "deviceId";
    case DeviceManager::CommandType::DeviceModel:
    case DeviceManager::CommandType::DeviceFirmware:
    case DeviceManager::CommandType::DeviceLocale:
        return String();
    case DeviceManager::CommandType::DevicePower:
        return "devicePower";
    case DeviceManager::CommandType::DeviceBatteryVoltage:
        return "deviceBatteryVoltage";
    case DeviceManager::CommandType::DeviceBatteryPercent:
        return "deviceBatteryPercent";
    case DeviceManager::CommandType::DeviceTime:
        return "deviceTime";
    case DeviceManager::CommandType::DeviceTimeZone:
        return "deviceTimeZone";
    case DeviceManager::CommandType::DeviceSensitivity:
        return "tubeSensitivity";
    case DeviceManager::CommandType::TubeTime:
        return "tubeLifetime";
    case DeviceManager::CommandType::TubePulseCount:
        return "tubePulseCount";
    case DeviceManager::CommandType::TubeRate:
        return "tubeRate";
    case DeviceManager::CommandType::TubeDoseRate:
        return "tubeDoseRate";
    case DeviceManager::CommandType::TubeDeadTime:
        return "tubeDeadTime";
    case DeviceManager::CommandType::TubeDeadTimeCompensation:
        return "tubeDeadTimeCompensation";
    case DeviceManager::CommandType::TubeHVFrequency:
        return "tubeHvFrequency";
    case DeviceManager::CommandType::TubeHVDutyCycle:
        return "tubeHvDutyCycle";
    case DeviceManager::CommandType::RandomData:
        return "randomData";
    case DeviceManager::CommandType::DataLog:
        return "dataLog";
    default:
        return String();
    }
}

String MqttPublisher::makeSlug(const String &raw) const
{
    String slug;
    slug.reserve(raw.length());
    for (size_t i = 0; i < raw.length(); ++i)
    {
        char c = raw[i];
        if (isalnum(static_cast<unsigned char>(c)))
        {
            slug += static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }
        else if (c == '-' || c == '_')
        {
            slug += c;
        }
        else if (isspace(static_cast<unsigned char>(c)) || c == ':' || c == '.' || c == '/')
        {
            if (!slug.endsWith("-"))
                slug += '-';
        }
    }
    while (slug.endsWith("-"))
        slug.remove(slug.length() - 1);
    if (!slug.length())
        slug = fallbackId_;
    return slug;
}

String MqttPublisher::sanitizedDeviceId() const
{
    if (deviceSlug_.length())
        return deviceSlug_;
    return fallbackId_;
}

bool MqttPublisher::publish(const String &leaf, const String &payload, bool retain)
{
    if (!config_.mqttEnabled)
    {
        if (publishCallback_)
            publishCallback_(true);
        return true;
    }

    if (!leaf.length())
    {
        if (publishCallback_)
            publishCallback_(false);
        return false;
    }

    if (!configValid_)
    {
        if (publishCallback_)
            publishCallback_(false);
        return false;
    }

    if (topicDirty_)
        refreshTopics();

    if (WiFi.status() != WL_CONNECTED)
    {
        unsigned long now = millis();
        if (now - lastPublishWarning_ > 5000)
        {
            log_.println("MQTT publish skipped: Wi-Fi disconnected.");
            lastPublishWarning_ = now;
        }
        led_.activateFault(FaultCode::MqttConnectionReset);
        return false;
    }

    if (!ensureConnected())
    {
        unsigned long now = millis();
        if (now - lastPublishWarning_ > 5000)
        {
            log_.println("MQTT publish skipped: not connected.");
            lastPublishWarning_ = now;
        }
        if (publishCallback_)
            publishCallback_(false);
        return false;
    }

    String topic = buildTopic(leaf);
    bool ok = mqtt_client_.publish(topic.c_str(), payload.c_str(), retain);
    if (!ok)
        led_.activateFault(FaultCode::MqttConnectionReset);
    else
        led_.clearFault(FaultCode::MqttConnectionReset);
    if (publishCallback_)
        publishCallback_(ok);
    return ok;
}

void MqttPublisher::publishDiscovery()
{
    if (discoveryPublished_ || !configValid_ || !mqtt_client_.connected())
        return;

    if (!deviceId_.length())
        return;

    if (!versionDiscoveryDone_)
    {
        if (!publishVersionDiscovery())
            return;
        versionDiscoveryDone_ = true;
    }

    if (topicDirty_)
        refreshTopics();

    if (!ensureConnected())
        return;

    struct DiscoveryEntry
    {
        DeviceManager::CommandType type;
        const char *component;
        const char *objectId;
        const char *name;
        const char *unit;
        const char *deviceClass;
        const char *stateClass;
        const char *payloadOn;
        const char *payloadOff;
    };

    static const DiscoveryEntry kEntities[] = {
        {DeviceManager::CommandType::DevicePower, "binary_sensor", "power", "Power", nullptr, "power", nullptr, "ON", "OFF"},
        {DeviceManager::CommandType::DeviceBatteryVoltage, "sensor", "battery_voltage", "Battery Voltage", "V", "voltage", "measurement", nullptr, nullptr},
        {DeviceManager::CommandType::DeviceBatteryPercent, "sensor", "battery", "Battery", "%", "battery", "measurement", nullptr, nullptr},
        {DeviceManager::CommandType::TubeRate, "sensor", "tube_rate", "Tube Rate", "cpm", nullptr, "measurement", nullptr, nullptr},
        {DeviceManager::CommandType::TubeDoseRate, "sensor", "tube_dose_rate", "Dose Rate", "µSv/h", nullptr, "measurement", nullptr, nullptr},
        {DeviceManager::CommandType::TubePulseCount, "sensor", "tube_pulse_count", "Tube Pulse Count", nullptr, nullptr, "total_increasing", nullptr, nullptr},
        {DeviceManager::CommandType::DeviceSensitivity, "sensor", "tube_sensitivity", "Tube Sensitivity", "cpm/µSv/h", nullptr, nullptr, nullptr, nullptr},
        {DeviceManager::CommandType::TubeDeadTime, "sensor", "tube_dead_time", "Tube Dead Time", "s", nullptr, nullptr, nullptr, nullptr},
        {DeviceManager::CommandType::TubeHVFrequency, "sensor", "tube_hv_frequency", "Tube HV Frequency", "Hz", "frequency", "measurement", nullptr, nullptr},
        {DeviceManager::CommandType::TubeHVDutyCycle, "sensor", "tube_hv_duty_cycle", "Tube HV Duty Cycle", nullptr, nullptr, nullptr, nullptr, nullptr}};

    size_t entityCount = sizeof(kEntities) / sizeof(kEntities[0]);
    if (discoveryIndex_ >= entityCount)
    {
        discoveryPublished_ = true;
        return;
    }

    unsigned long now = millis();
    if (lastDiscoveryAttempt_ != 0 && now - lastDiscoveryAttempt_ < 1000)
        return;
    lastDiscoveryAttempt_ = now;

    const auto &entry = kEntities[discoveryIndex_];
    if (publishDiscoveryEntity(entry.type,
                               entry.component,
                               entry.objectId,
                               entry.name,
                               entry.unit,
                               entry.deviceClass,
                               entry.stateClass,
                               entry.payloadOn,
                               entry.payloadOff))
    {
        discoveryIndex_++;
        if (discoveryIndex_ >= entityCount)
            discoveryPublished_ = true;
    }
}

bool MqttPublisher::publishDiscoveryEntity(DeviceManager::CommandType type,
                                           const char *component,
                                           const char *objectId,
                                           const char *name,
                                           const char *unit,
                                           const char *deviceClass,
                                           const char *stateClass,
                                           const char *payloadOn,
                                           const char *payloadOff,
                                           const char *entityCategory,
                                           const char *leafOverride)
{
    String leaf = (leafOverride && *leafOverride) ? String(leafOverride) : commandLeaf(type);
    if (!leaf.length())
        return true;

    String stateTopic = buildTopic(leaf);
    if (!stateTopic.length())
        return false;

    String deviceIdSlug = sanitizedDeviceId();
    String discoveryTopic = String("homeassistant/") + component + "/" + deviceIdSlug + "/" + objectId + "/config";
    String objectUid = deviceIdSlug + "_" + objectId;

    String deviceName = deviceNameForDiscovery();
    String fullName;
    if (name && *name)
        fullName = name;
    else
        fullName = deviceName;

    String payload;
    payload.reserve(256);
    payload = "{";

    auto appendField = [&](const char *field, const String &val) {
        if (!val.length())
            return;
        if (payload.length() > 1)
            payload += ',';
        payload += '"';
        payload += field;
        payload += "\":\"";
        payload += escapeJson(val);
        payload += '"';
    };

    appendField("name", fullName);
    appendField("state_topic", stateTopic);
    appendField("unique_id", objectUid);
    String deviceNameSlug = makeSlug(deviceName);
    String objectIdField;
    if (deviceNameSlug.length())
    {
        objectIdField = deviceNameSlug;
        objectIdField += "_";
        objectIdField += objectId;
    }
    else
    {
        objectIdField = objectUid;
    }
    appendField("object_id", objectIdField);
    if (unit && *unit)
        appendField("unit_of_measurement", String(unit));
    if (deviceClass && *deviceClass)
        appendField("device_class", String(deviceClass));
    if (stateClass && *stateClass)
        appendField("state_class", String(stateClass));
    if (payloadOn && *payloadOn)
    {
        appendField("payload_on", String(payloadOn));
        if (payloadOff && *payloadOff)
            appendField("payload_off", String(payloadOff));
    }
    if (entityCategory && *entityCategory)
        appendField("entity_category", String(entityCategory));

    String deviceJson = "{";
    bool firstDeviceField = true;
    auto appendDeviceField = [&](const char *field, const String &val) {
        if (!val.length())
            return;
        if (!firstDeviceField)
            deviceJson += ',';
        deviceJson += '"';
        deviceJson += field;
        deviceJson += "\":\"";
        deviceJson += escapeJson(val);
        deviceJson += '"';
        firstDeviceField = false;
    };

    String identifier = String("radpro-") + deviceIdSlug;
    deviceJson += "\"identifiers\":[\"" + escapeJson(identifier) + "\"]";
    firstDeviceField = false;
    appendDeviceField("manufacturer", String("Bosean"));
    appendDeviceField("model", deviceModelForDiscovery());
    appendDeviceField("name", deviceNameForDiscovery());
    if (deviceFirmware_.length())
        appendDeviceField("sw_version", deviceFirmware_);
    deviceJson += "}";

    if (payload.length() > 1)
        payload += ',';
    payload += "\"device\":";
    payload += deviceJson;
    payload += "}";

    size_t neededLen = discoveryTopic.length() + payload.length() + 16;
    if (neededLen > mqtt_client_.getBufferSize())
    {
        log_.print("MQTT discovery payload too large for ");
        log_.println(discoveryTopic);
        log_.print("Required bytes: ");
        log_.println(neededLen);
        log_.print("Buffer size: ");
        log_.println(mqtt_client_.getBufferSize());
        led_.activateFault(FaultCode::MqttDiscoveryTooLarge);
        return false;
    }

    bool ok = mqtt_client_.publish(discoveryTopic.c_str(), payload.c_str(), true);
    if (!ok)
    {
        log_.print("MQTT discovery publish failed for ");
        log_.println(discoveryTopic);
        led_.activateFault(FaultCode::MqttConnectionReset);
    }
    else
    {
        led_.clearFault(FaultCode::MqttDiscoveryTooLarge);
    }
    return ok;
}

bool MqttPublisher::publishVersionDiscovery()
{
    if (!bridgeVersion_.length())
        return true;

    return publishDiscoveryEntity(DeviceManager::CommandType::DeviceId,
                                  "sensor",
                                  "bridge_version",
                                  "Bridge Firmware Version",
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  "diagnostic",
                                  "bridgeVersion");
}

bool MqttPublisher::publishBridgeVersion()
{
    if (!bridgeVersionDirty_)
        return true;

    if (!bridgeVersion_.length())
    {
        bridgeVersionDirty_ = false;
        return true;
    }

    bool ok = publish("bridgeVersion", bridgeVersion_, true);
    if (ok)
        bridgeVersionDirty_ = false;
    return ok;
}

String MqttPublisher::escapeJson(const String &value) const
{
    String out;
    out.reserve(value.length() + 4);
    for (size_t i = 0; i < value.length(); ++i)
    {
        char c = value[i];
        switch (c)
        {
        case '\\':
        case '"':
            out += '\\';
            out += c;
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char buf[7];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            }
            else
            {
                out += c;
            }
            break;
        }
    }
    return out;
}

String MqttPublisher::deviceNameForDiscovery() const
{
    if (config_.deviceName.length())
        return config_.deviceName;
    if (deviceModel_.length())
        return deviceModel_;
    if (deviceId_.length())
        return deviceId_;
    return String("RadPro WiFi Bridge");
}

String MqttPublisher::deviceModelForDiscovery() const
{
    if (deviceModel_.length())
        return deviceModel_;
    return String("RadPro FS-600");
}
MqttPublisher::RetainedState *MqttPublisher::retainedEntry(DeviceManager::CommandType type)
{
    for (auto &state : retainedStates_)
    {
        if (state.type == type)
            return &state;
    }
    return nullptr;
}

void MqttPublisher::markAllPending()
{
    for (auto &state : retainedStates_)
    {
        if (state.hasValue)
            state.pending = true;
    }
    lastRepublishAttempt_ = 0;
    bridgeVersionDirty_ = true;
}

void MqttPublisher::republishRetained()
{
    if (!mqtt_client_.connected())
        return;

    if (topicDirty_)
        refreshTopics();

    unsigned long now = millis();
    if (lastRepublishAttempt_ != 0 && now - lastRepublishAttempt_ < 1000)
        return;
    lastRepublishAttempt_ = now;

    for (auto &state : retainedStates_)
    {
        if (!state.hasValue || !state.pending)
            continue;

        String leaf = commandLeaf(state.type);
        if (!leaf.length())
        {
            state.pending = false;
            continue;
        }

        String topic = buildTopic(leaf);
        bool ok = mqtt_client_.publish(topic.c_str(), state.payload.c_str(), true);
        if (ok)
        {
            state.pending = false;
        }
        else
        {
            if (publishCallback_)
                publishCallback_(false);
            lastRepublishAttempt_ = now; // retry later
            break;
        }
    }

    if (bridgeVersionDirty_)
        publishBridgeVersion();
}

void MqttPublisher::setBridgeVersion(const String &version)
{
    String trimmed = version;
    trimmed.trim();
    if (bridgeVersion_ == trimmed)
        return;

    bridgeVersion_ = trimmed;
    bridgeVersionDirty_ = true;
    versionDiscoveryDone_ = false;
    discoveryPublished_ = false;
    lastDiscoveryAttempt_ = 0;

    if (mqtt_client_.connected())
        publishBridgeVersion();
}
