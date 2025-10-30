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

void LedController::activateFault(FaultCode code)
{
    size_t idx = static_cast<size_t>(code);
    if (idx == 0 || idx >= faultActive_.size())
        return;
    bool hadFault = hasFault();
    faultActive_[idx] = true;
    if (!hadFault || currentFault() == code)
        resetFaultPattern();
}

void LedController::clearFault(FaultCode code)
{
    size_t idx = static_cast<size_t>(code);
    if (idx == 0 || idx >= faultActive_.size())
        return;
    bool wasCurrent = (currentFault() == code);
    faultActive_[idx] = false;
    if (wasCurrent)
        resetFaultPattern();
}

void LedController::clearAllFaults()
{
    faultActive_.fill(false);
    resetFaultPattern();
}

bool LedController::hasFault() const
{
    return currentFault() != FaultCode::None;
}

FaultCode LedController::currentFault() const
{
    for (size_t i = 1; i < faultActive_.size(); ++i)
    {
        if (faultActive_[i])
            return static_cast<FaultCode>(i);
    }
    return FaultCode::None;
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
    FaultCode fault = currentFault();
    if (fault != FaultCode::None)
    {
        updateFaultPattern(now, fault);
        return;
    }

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

void LedController::resetFaultPattern()
{
    faultStep_ = 0;
    faultNextMs_ = 0;
}

void LedController::updateFaultPattern(uint32_t now, FaultCode code)
{
    uint8_t issueIndex = static_cast<uint8_t>(code);
    if (issueIndex == 0)
        return;

    uint8_t orangeCount = issueIndex;
    uint8_t totalPairs = static_cast<uint8_t>(1 + orangeCount);
    uint8_t totalSteps = static_cast<uint8_t>(totalPairs * 2 + 1);

    if (faultNextMs_ == 0 || now >= faultNextMs_)
    {
        if (faultStep_ >= totalSteps)
            faultStep_ = 0;

        uint8_t pairIndex = faultStep_ / 2;
        bool onStep = (faultStep_ % 2 == 0) && (faultStep_ < totalPairs * 2);

        if (onStep)
        {
            Color color;
            if (pairIndex == 0)
            {
                color = {static_cast<uint8_t>(brightness_ * 3), 0, 0};
            }
            else
            {
                color = {static_cast<uint8_t>(brightness_ * 3), static_cast<uint8_t>(brightness_ * 2), 0};
            }
            applyColor(color);
            faultNextMs_ = now + 180;
        }
        else
        {
            applyColor({0, 0, 0});
            if (faultStep_ == totalSteps - 1)
                faultNextMs_ = now + 500;
            else
                faultNextMs_ = now + 140;
        }

        ++faultStep_;
    }
}
