#include "DeviceInfoStore.h"
#include <ArduinoJson.h>

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

String DeviceInfoStore::toJson() const
{
    DeviceInfoSnapshot snap = snapshot();
    JsonDocument doc;
    doc["manufacturer"] = snap.manufacturer;
    doc["model"] = snap.model;
    doc["firmware"] = snap.firmware;
    doc["bridgeFirmware"] = snap.bridgeFirmware;
    doc["deviceId"] = snap.deviceId;
    doc["locale"] = snap.locale;
    doc["devicePower"] = snap.devicePower.length() ? snap.devicePower.c_str() : nullptr;
    doc["batteryVoltage"] = snap.batteryVoltage.length() ? snap.batteryVoltage.c_str() : nullptr;
    doc["batteryPercent"] = snap.batteryPercent.length() ? snap.batteryPercent.c_str() : nullptr;
    doc["tubeRate"] = snap.tubeRate.length() ? snap.tubeRate.c_str() : nullptr;
    doc["tubeDoseRate"] = snap.tubeDoseRate.length() ? snap.tubeDoseRate.c_str() : nullptr;
    doc["tubePulseCount"] = snap.tubePulseCount.length() ? snap.tubePulseCount.c_str() : nullptr;
    if (snap.measurementAgeMs > 0)
        doc["measurementAgeMs"] = snap.measurementAgeMs;
    else
        doc["measurementAgeMs"] = nullptr;

    String json;
    serializeJson(doc, json);
    return json;
}
