#pragma once

#include <Arduino.h>
#include <array>

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

enum class FaultCode : uint8_t
{
    None = 0,
    NvsLoadFailure,
    NvsWriteFailure,
    WifiAuthFailure,
    WifiDhcpFailure,
    WifiPortalStuck,
    MqttUnreachable,
    MqttAuthFailure,
    MqttConnectionReset,
    MqttDiscoveryTooLarge,
    UsbDeviceGone,
    UsbInterfaceFailure,
    UsbHandshakeUnsupported,
    DeviceIdTimeout,
    CommandTimeout,
    MissingSensitivity,
    PortalReconnectFailed,
    PortalHeapExhausted,
    LedStateStuck,
    BuildSizeExceeded,
    UploadPortMissing,
    HaDiscoveryStale,
    HaRetainMissing,
    PowerBrownout,
    WatchdogReset,
    FaultCount
};

class LedController
{
public:
    void begin();
    void setMode(LedMode mode);
    void triggerPulse(LedPulse pulse, uint32_t durationMs = 200);
    void update();
    void setBrightness(uint8_t brightness) { brightness_ = brightness; }
    void activateFault(FaultCode code);
    void clearFault(FaultCode code);
    void clearAllFaults();
    bool hasFault() const;
    FaultCode currentFault() const;

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
    void resetFaultPattern();
    void expireFaults(uint32_t now);
    void updateFaultPattern(uint32_t now, FaultCode code);

    LedMode mode_ = LedMode::Booting;
    LedPulse pulse_ = LedPulse::None;
    uint32_t pulseEndMs_ = 0;
    uint8_t brightness_ = 8;
    Color lastColor_{0, 0, 0};
    std::array<bool, static_cast<size_t>(FaultCode::FaultCount)> faultActive_{};
    std::array<uint32_t, static_cast<size_t>(FaultCode::FaultCount)> faultActivatedAtMs_{};
    uint8_t faultStep_ = 0;
    uint32_t faultNextMs_ = 0;
};
