// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include <string>

#include "BridgeDiagnostics.h"
#include "Led/LedController.h"
#include "WiFi.h"

FaultCode faultCodeForMqttConnectState(int state);

class BufferPrint : public Print
{
public:
    size_t write(uint8_t ch) override
    {
        buffer_.push_back(static_cast<char>(ch));
        return 1;
    }

    const std::string &buffer() const
    {
        return buffer_;
    }

private:
    std::string buffer_;
};

static void testMqttErrorDoesNotOverrideHealthyBridge()
{
    BufferPrint log;
    LedController led;
    BridgeDiagnostics diagnostics(log, led);

    WiFi.setStatus(WL_CONNECTED);
    diagnostics.updateLedStatus(true, false, true, true);

    assert(led.currentModeForDebug() == LedMode::DeviceReady);
}

static void testMqttErrorDoesNotOverrideWifiConnectedIdleBridge()
{
    BufferPrint log;
    LedController led;
    BridgeDiagnostics diagnostics(log, led);

    WiFi.setStatus(WL_CONNECTED);
    diagnostics.updateLedStatus(true, false, true, false);

    assert(led.currentModeForDebug() == LedMode::WifiConnected);
}

static void testOnlyMqttAuthFailureStaysLatched()
{
    assert(faultCodeForMqttConnectState(5) == FaultCode::MqttAuthFailure);
    assert(faultCodeForMqttConnectState(-2) == FaultCode::None);
    assert(faultCodeForMqttConnectState(-3) == FaultCode::None);
    assert(faultCodeForMqttConnectState(-4) == FaultCode::None);
}

int main()
{
    testMqttErrorDoesNotOverrideHealthyBridge();
    testMqttErrorDoesNotOverrideWifiConnectedIdleBridge();
    testOnlyMqttAuthFailureStaysLatched();
    std::cout << "bridge LED policy tests passed\n";
    return 0;
}
