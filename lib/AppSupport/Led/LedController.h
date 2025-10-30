#pragma once

#include <Arduino.h>

enum class LedMode
{
    Booting,
    WaitingForStart,
    WifiConnecting,
    WifiConnected,
    DeviceReady,
    Error
};

enum class LedPulse
{
    None,
    MqttSuccess,
    MqttFailure
};

class LedController
{
public:
    void begin();
    void setMode(LedMode mode);
    void triggerPulse(LedPulse pulse, uint32_t durationMs = 200);
    void update();
    void setBrightness(uint8_t brightness) { brightness_ = brightness; }

private:
    struct Color
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    Color colorForMode(LedMode mode, uint32_t now) const;
    Color colorForPulse(LedPulse pulse) const;
    void applyColor(const Color &color);

    LedMode mode_ = LedMode::Booting;
    LedPulse pulse_ = LedPulse::None;
    uint32_t pulseEndMs_ = 0;
    uint8_t brightness_ = 8;
    Color lastColor_{0, 0, 0};
};
