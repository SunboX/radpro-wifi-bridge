// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>

#include "UsbRecoveryPolicy.h"

namespace
{
void testRequiresDisconnectGracePeriod()
{
    assert(!UsbRecoveryPolicy::shouldRestart(1000, 0, 0, true, false));
    assert(!UsbRecoveryPolicy::shouldRestart(14999, 1, 0, true, false));
    assert(UsbRecoveryPolicy::shouldRestart(15000, 0, 0, true, false) == false);
    assert(UsbRecoveryPolicy::shouldRestart(16000, 1000, 0, true, false));
}

void testRespectsRestartInterval()
{
    const unsigned long disconnectedSince = 1000;
    assert(!UsbRecoveryPolicy::shouldRestart(20000, disconnectedSince, 10000, true, false));
    assert(!UsbRecoveryPolicy::shouldRestart(39999, disconnectedSince, 10000, true, false));
    assert(UsbRecoveryPolicy::shouldRestart(40000, disconnectedSince, 10000, true, false));
}

void testBlockingRestartRequiresAnObservedUsbDevice()
{
    assert(!UsbRecoveryPolicy::shouldRestart(60000, 1000, 0, false, false));
}

void testBackgroundRestartStillRequiresAnObservedUsbDevice()
{
    assert(!UsbRecoveryPolicy::shouldRestart(60000, 1000, 0, false, true));
    assert(UsbRecoveryPolicy::shouldRestart(60000, 1000, 0, true, true));
}

void testLateAttachGetsItsOwnOpeningGracePeriod()
{
    // USB has been absent since boot, but was observed only just now.
    constexpr unsigned long readyAt = 120250;
    assert(!UsbRecoveryPolicy::shouldRestart(120000, 1000, 0, true, true, readyAt));
    assert(!UsbRecoveryPolicy::shouldRestart(135249, 1000, 0, true, true, readyAt));
    assert(UsbRecoveryPolicy::shouldRestart(135250, 1000, 0, true, true, readyAt));
    // Once the device is gone, the original disconnect/retry policy applies.
    assert(UsbRecoveryPolicy::shouldRestart(160000, 140000, 0, true, true, 0));
}
} // namespace

int main()
{
    testRequiresDisconnectGracePeriod();
    testRespectsRestartInterval();
    testBlockingRestartRequiresAnObservedUsbDevice();
    testBackgroundRestartStillRequiresAnObservedUsbDevice();
    testLateAttachGetsItsOwnOpeningGracePeriod();
    std::cout << "usb recovery policy tests passed\n";
    return 0;
}
