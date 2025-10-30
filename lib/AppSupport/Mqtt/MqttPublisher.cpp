#include "MqttPublisher.h"

#include <ctype.h>

MqttPublisher::MqttPublisher(AppConfig &config, Print &log)
    : config_(config),
      log_(log),
      mqtt_client_(wifi_client_)
{
    uint64_t mac = ESP.getEfuseMac();
    char buf[17];
    snprintf(buf, sizeof(buf), "%012llx", static_cast<unsigned long long>(mac));
    fallbackId_ = String("esp32s3-") + buf;
}

void MqttPublisher::begin()
{
    mqtt_client_.setBufferSize(512);
    updateConfig();
}

void MqttPublisher::updateConfig()
{
    String host = config_.mqttHost;
    host.trim();
    uint16_t port = config_.mqttPort ? config_.mqttPort : 1883;

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
    if (!configValid_)
        return;

    if (!mqtt_client_.connected())
    {
        ensureConnected();
    }

    if (mqtt_client_.connected())
    {
        mqtt_client_.loop();
    }
}

void MqttPublisher::onCommandResult(DeviceManager::CommandType type, const String &value)
{
    String leaf = commandLeaf(type);
    if (!leaf.length())
        return;

    if (type == DeviceManager::CommandType::DeviceId)
    {
        if (!value.length())
            return;
        deviceId_ = value;
        deviceSlug_ = makeSlug(value);
        topicDirty_ = true;
        publish(leaf, value, true);
        return;
    }

    if (type == DeviceManager::CommandType::DevicePower)
    {
        String payload = (value == "1" ? "ON" : (value == "0" ? "OFF" : value));
        publish(leaf, payload, true);
        return;
    }

    bool retain = true;
    if (type == DeviceManager::CommandType::RandomData || type == DeviceManager::CommandType::DataLog)
        retain = false;

    publish(leaf, value, retain);
}

bool MqttPublisher::ensureConnected()
{
    if (!configValid_ || currentHost_.isEmpty())
        return false;

    if (mqtt_client_.connected())
        return true;

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

    if (connected)
    {
        log_.println("MQTT connected.");
    }
    else
    {
        log_.print("MQTT connect failed: ");
        log_.println(mqtt_client_.state());
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
    case DeviceManager::CommandType::DevicePower:
        return "devicePower";
    case DeviceManager::CommandType::DeviceBatteryVoltage:
        return "deviceBatteryVoltage";
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

void MqttPublisher::publish(const String &leaf, const String &payload, bool retain)
{
    if (!leaf.length())
        return;

    if (!configValid_)
        return;

    if (topicDirty_)
        refreshTopics();

    if (!ensureConnected())
    {
        unsigned long now = millis();
        if (now - lastPublishWarning_ > 5000)
        {
            log_.println("MQTT publish skipped: not connected.");
            lastPublishWarning_ = now;
        }
        return;
    }

    String topic = buildTopic(leaf);
    mqtt_client_.publish(topic.c_str(), payload.c_str(), retain);
}
