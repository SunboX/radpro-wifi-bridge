#include "Led/LedController.h"

#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48
#endif

void LedController::begin()
{
    applyColor({0, 0, 0});
}

void LedController::setMode(LedMode mode)
{
    if (mode_ == mode)
        return;
    mode_ = mode;
}

void LedController::triggerPulse(LedPulse pulse, uint32_t durationMs)
{
    pulse_ = pulse;
    pulseEndMs_ = millis() + durationMs;
}

LedController::Color LedController::colorForMode(LedMode mode, uint32_t now) const
{
    switch (mode)
    {
    case LedMode::Booting:
        return (now / 400 % 2 == 0) ? Color{brightness_, 0, brightness_} : Color{0, 0, 0};
    case LedMode::WaitingForStart:
        return (now / 600 % 2 == 0) ? Color{brightness_, brightness_, 0} : Color{0, 0, 0};
    case LedMode::WifiConnecting:
        return (now / 600 % 2 == 0) ? Color{0, 0, brightness_} : Color{0, 0, 0};
    case LedMode::WifiConnected:
        return Color{0, brightness_, brightness_};
    case LedMode::DeviceReady:
        return Color{0, static_cast<uint8_t>(brightness_ * 2), 0};
    case LedMode::Error:
        return (now / 500 % 2 == 0) ? Color{static_cast<uint8_t>(brightness_ * 2), brightness_, 0} : Color{0, 0, 0};
    }
    return Color{0, 0, 0};
}

LedController::Color LedController::colorForPulse(LedPulse pulse) const
{
    switch (pulse)
    {
    case LedPulse::MqttSuccess:
        return Color{0, static_cast<uint8_t>(brightness_ * 3), 0};
    case LedPulse::MqttFailure:
        return Color{static_cast<uint8_t>(brightness_ * 3), 0, 0};
    default:
        return Color{0, 0, 0};
    }
}

void LedController::applyColor(const Color &color)
{
    if (color.r == lastColor_.r && color.g == lastColor_.g && color.b == lastColor_.b)
        return;
    neopixelWrite(RGB_BUILTIN, color.r, color.g, color.b);
    lastColor_ = color;
}

void LedController::update()
{
    uint32_t now = millis();
    Color color = colorForMode(mode_, now);

    if (pulse_ != LedPulse::None)
    {
        if (static_cast<int32_t>(pulseEndMs_ - now) > 0)
        {
            color = colorForPulse(pulse_);
        }
        else
        {
            pulse_ = LedPulse::None;
        }
    }

    applyColor(color);
}
