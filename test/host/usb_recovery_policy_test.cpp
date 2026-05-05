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
    assert(!UsbRecoveryPolicy::shouldRestart(1000, 0, 0));
    assert(!UsbRecoveryPolicy::shouldRestart(14999, 1, 0));
    assert(UsbRecoveryPolicy::shouldRestart(15000, 0, 0) == false);
    assert(UsbRecoveryPolicy::shouldRestart(16000, 1000, 0));
}

void testRespectsRestartInterval()
{
    const unsigned long disconnectedSince = 1000;
    assert(!UsbRecoveryPolicy::shouldRestart(20000, disconnectedSince, 10000));
    assert(!UsbRecoveryPolicy::shouldRestart(39999, disconnectedSince, 10000));
    assert(UsbRecoveryPolicy::shouldRestart(40000, disconnectedSince, 10000));
}
} // namespace

int main()
{
    testRequiresDisconnectGracePeriod();
    testRespectsRestartInterval();
    std::cout << "usb recovery policy tests passed\n";
    return 0;
}
