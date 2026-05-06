/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

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
    void setMode(LedMode mode)
    {
        mode_ = mode;
    }

    void activateFault(FaultCode code)
    {
        lastActivatedFault_ = code;
    }

    void clearFault(FaultCode code)
    {
        lastClearedFault_ = code;
    }

    LedMode currentModeForDebug() const
    {
        return mode_;
    }

    FaultCode lastActivatedFault() const
    {
        return lastActivatedFault_;
    }

    FaultCode lastClearedFault() const
    {
        return lastClearedFault_;
    }

private:
    LedMode mode_ = LedMode::Booting;
    FaultCode lastActivatedFault_ = FaultCode::None;
    FaultCode lastClearedFault_ = FaultCode::None;
};
