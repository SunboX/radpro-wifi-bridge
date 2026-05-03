#pragma once

#include <Arduino.h>
#include <cstdlib>
#include "DeviceManager.h"

enum class DeviceActivityFault
{
    None,
    PowerOff,
    StalePulseCount,
    TelemetryTimeout
};

class DeviceActivityMonitor
{
public:
    static constexpr uint32_t kDefaultStalePulseTimeoutMs = 120000;
    static constexpr uint32_t kDefaultMissingTelemetryTimeoutMs = 15000;

    void reset()
    {
        powerOff_ = false;
        stalePulseCount_ = false;
        telemetryTimeout_ = false;
        havePulseCount_ = false;
        haveTelemetry_ = false;
        consecutiveTelemetryFailures_ = 0;
        lastPulseCount_ = 0;
        lastPulseChangedMs_ = 0;
        lastTelemetrySuccessMs_ = 0;
    }

    void setStalePulseTimeoutMs(uint32_t timeoutMs)
    {
        stalePulseTimeoutMs_ = timeoutMs;
    }

    void setMissingTelemetryTimeoutMs(uint32_t timeoutMs)
    {
        missingTelemetryTimeoutMs_ = timeoutMs;
    }

    DeviceActivityFault onCommandResult(DeviceManager::CommandType type,
                                        const String &value,
                                        bool success,
                                        unsigned long now)
    {
        if (!success)
        {
            if (isTelemetryCommand(type))
            {
                ++consecutiveTelemetryFailures_;
                if (haveTelemetry_ && consecutiveTelemetryFailures_ >= 2 &&
                    missingTelemetryTimeoutMs_ > 0 &&
                    static_cast<unsigned long>(now - lastTelemetrySuccessMs_) >= missingTelemetryTimeoutMs_)
                {
                    telemetryTimeout_ = true;
                }
            }
            return evaluate(now);
        }

        if (isTelemetryCommand(type))
        {
            haveTelemetry_ = true;
            consecutiveTelemetryFailures_ = 0;
            lastTelemetrySuccessMs_ = now;
            telemetryTimeout_ = false;
        }

        if (type == DeviceManager::CommandType::DevicePower)
        {
            if (value == "0")
            {
                powerOff_ = true;
            }
            else if (value == "1")
            {
                powerOff_ = false;
                stalePulseCount_ = false;
                telemetryTimeout_ = false;
                havePulseCount_ = false;
                lastPulseCount_ = 0;
                lastPulseChangedMs_ = now;
            }
            return fault();
        }

        if (type == DeviceManager::CommandType::TubePulseCount)
        {
            unsigned long pulseCount = 0;
            if (!parseUnsignedLong(value, pulseCount))
                return evaluate(now);

            if (!havePulseCount_ || pulseCount != lastPulseCount_)
            {
                havePulseCount_ = true;
                lastPulseCount_ = pulseCount;
                lastPulseChangedMs_ = now;
                stalePulseCount_ = false;
            }
            else
            {
                evaluate(now);
            }
        }

        return fault();
    }

    DeviceActivityFault evaluate(unsigned long now)
    {
        if (!powerOff_ && havePulseCount_ && stalePulseTimeoutMs_ > 0 &&
            static_cast<unsigned long>(now - lastPulseChangedMs_) >= stalePulseTimeoutMs_)
        {
            stalePulseCount_ = true;
        }
        return fault();
    }

    DeviceActivityFault fault() const
    {
        if (powerOff_)
            return DeviceActivityFault::PowerOff;
        if (stalePulseCount_)
            return DeviceActivityFault::StalePulseCount;
        if (telemetryTimeout_)
            return DeviceActivityFault::TelemetryTimeout;
        return DeviceActivityFault::None;
    }

    bool hasFault() const
    {
        return fault() != DeviceActivityFault::None;
    }

    bool shouldSuppressTelemetry(DeviceManager::CommandType type) const
    {
        if (!hasFault())
            return false;

        switch (type)
        {
        case DeviceManager::CommandType::TubePulseCount:
        case DeviceManager::CommandType::TubeRate:
        case DeviceManager::CommandType::TubeDoseRate:
            return true;
        default:
            return false;
        }
    }

private:
    static bool isTelemetryCommand(DeviceManager::CommandType type)
    {
        switch (type)
        {
        case DeviceManager::CommandType::DevicePower:
        case DeviceManager::CommandType::DeviceBatteryVoltage:
        case DeviceManager::CommandType::TubePulseCount:
        case DeviceManager::CommandType::TubeRate:
            return true;
        default:
            return false;
        }
    }

    static bool parseUnsignedLong(const String &value, unsigned long &out)
    {
        if (!value.length())
            return false;
        char *end = nullptr;
        unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
        if (!end || *end != '\0')
            return false;
        out = parsed;
        return true;
    }

    bool powerOff_ = false;
    bool stalePulseCount_ = false;
    bool telemetryTimeout_ = false;
    bool havePulseCount_ = false;
    bool haveTelemetry_ = false;
    uint8_t consecutiveTelemetryFailures_ = 0;
    unsigned long lastPulseCount_ = 0;
    unsigned long lastPulseChangedMs_ = 0;
    unsigned long lastTelemetrySuccessMs_ = 0;
    uint32_t stalePulseTimeoutMs_ = kDefaultStalePulseTimeoutMs;
    uint32_t missingTelemetryTimeoutMs_ = kDefaultMissingTelemetryTimeoutMs;
};
