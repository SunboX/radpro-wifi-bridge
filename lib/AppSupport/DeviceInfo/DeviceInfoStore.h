#pragma once

#include <Arduino.h>
#include "DeviceManager.h"
#include <freertos/FreeRTOS.h>

struct DeviceInfoSnapshot
{
    String manufacturer;
    String model;
    String firmware;
    String bridgeFirmware;
    String deviceId;
    String locale;
    String devicePower;
    String batteryVoltage;
    String batteryPercent;
    String tubeRate;
    String tubeDoseRate;
    String tubePulseCount;
    unsigned long measurementAgeMs = 0;
};

class DeviceInfoStore
{
public:
    DeviceInfoStore();

    void update(DeviceManager::CommandType type, const String &value);
    void setBridgeFirmware(const String &version);

    DeviceInfoSnapshot snapshot() const;
    String toJson() const;

private:
    void setManufacturerFromModel(const String &model);
    static String escapeJson(const String &value);

    mutable portMUX_TYPE mux_;
    String manufacturer_;
    String model_;
    String firmware_;
    String bridgeFirmware_;
    String deviceId_;
    String locale_;
    String devicePower_;
    String batteryVoltage_;
    String batteryPercent_;
    String tubeRate_;
    String tubeDoseRate_;
    String tubePulseCount_;
    unsigned long measurementUpdatedMs_ = 0;
};
