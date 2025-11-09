#include "DeviceInfoStore.h"

DeviceInfoStore::DeviceInfoStore()
    : mux_(portMUX_INITIALIZER_UNLOCKED),
      manufacturer_("RadPro"),
      bridgeFirmware_("unknown")
{
}

void DeviceInfoStore::setBridgeFirmware(const String &version)
{
    portENTER_CRITICAL(&mux_);
    bridgeFirmware_ = version;
    portEXIT_CRITICAL(&mux_);
}

void DeviceInfoStore::setManufacturerFromModel(const String &model)
{
    if (!model.length())
        return;
    int space = model.indexOf(' ');
    if (space > 0)
        manufacturer_ = model.substring(0, space);
    else
        manufacturer_ = model;
}

void DeviceInfoStore::update(DeviceManager::CommandType type, const String &value)
{
    unsigned long now = millis();
    portENTER_CRITICAL(&mux_);
    switch (type)
    {
    case DeviceManager::CommandType::DeviceId:
        deviceId_ = value;
        break;
    case DeviceManager::CommandType::DeviceModel:
        model_ = value;
        setManufacturerFromModel(value);
        break;
    case DeviceManager::CommandType::DeviceFirmware:
        firmware_ = value;
        break;
    case DeviceManager::CommandType::DeviceLocale:
        locale_ = value;
        break;
    case DeviceManager::CommandType::DevicePower:
        devicePower_ = value;
        break;
    case DeviceManager::CommandType::DeviceBatteryVoltage:
        batteryVoltage_ = value;
        break;
    case DeviceManager::CommandType::DeviceBatteryPercent:
        batteryPercent_ = value;
        break;
    case DeviceManager::CommandType::TubePulseCount:
        tubePulseCount_ = value;
        measurementUpdatedMs_ = now;
        break;
    case DeviceManager::CommandType::TubeRate:
        tubeRate_ = value;
        measurementUpdatedMs_ = now;
        break;
    case DeviceManager::CommandType::TubeDoseRate:
        tubeDoseRate_ = value;
        measurementUpdatedMs_ = now;
        break;
    default:
        break;
    }
    portEXIT_CRITICAL(&mux_);
}

DeviceInfoSnapshot DeviceInfoStore::snapshot() const
{
    DeviceInfoSnapshot snap;
    portENTER_CRITICAL(&mux_);
    snap.manufacturer = manufacturer_;
    snap.model = model_;
    snap.firmware = firmware_;
    snap.bridgeFirmware = bridgeFirmware_;
    snap.bridgeFirmware = bridgeFirmware_;
    snap.deviceId = deviceId_;
    snap.locale = locale_;
    snap.devicePower = devicePower_;
    snap.batteryVoltage = batteryVoltage_;
    snap.batteryPercent = batteryPercent_;
    snap.tubeRate = tubeRate_;
    snap.tubeDoseRate = tubeDoseRate_;
    snap.tubePulseCount = tubePulseCount_;
    if (measurementUpdatedMs_)
        snap.measurementAgeMs = millis() - measurementUpdatedMs_;
    else
        snap.measurementAgeMs = 0;
    portEXIT_CRITICAL(&mux_);
    return snap;
}

String DeviceInfoStore::escapeJson(const String &value)
{
    String out;
    out.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i)
    {
        char c = value[i];
        switch (c)
        {
        case '"':
        case '\\':
            out += '\\';
            out += c;
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
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
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

String DeviceInfoStore::toJson() const
{
    DeviceInfoSnapshot snap = snapshot();
    String json;
    json.reserve(512);
    json += '{';
    auto appendField = [&json](const char *key, const String &value) {
        json += '\"';
        json += key;
        json += '\"';
        json += ':';
        if (value.length())
        {
            json += '\"';
            json += DeviceInfoStore::escapeJson(value);
            json += '\"';
        }
        else
        {
            json += "null";
        }
        json += ',';
    };

    appendField("manufacturer", snap.manufacturer);
    appendField("model", snap.model);
    appendField("firmware", snap.firmware);
    appendField("bridgeFirmware", snap.bridgeFirmware);
    appendField("deviceId", snap.deviceId);
    appendField("locale", snap.locale);
    appendField("devicePower", snap.devicePower);
    appendField("batteryVoltage", snap.batteryVoltage);
    appendField("batteryPercent", snap.batteryPercent);
    appendField("tubeRate", snap.tubeRate);
    appendField("tubeDoseRate", snap.tubeDoseRate);
    appendField("tubePulseCount", snap.tubePulseCount);
    json += '\"';
    json += "measurementAgeMs";
    json += '\"';
    json += ':';
    if (snap.measurementAgeMs > 0)
        json += String(snap.measurementAgeMs);
    else
        json += "null";
    json += '}';
    return json;
}
