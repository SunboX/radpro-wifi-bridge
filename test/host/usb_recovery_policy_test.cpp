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

void testBackgroundRestartCanRecoverDescriptorFailures()
{
    assert(UsbRecoveryPolicy::shouldRestart(60000, 1000, 0, false, true));
}
} // namespace

int main()
{
    testRequiresDisconnectGracePeriod();
    testRespectsRestartInterval();
    testBlockingRestartRequiresAnObservedUsbDevice();
    testBackgroundRestartCanRecoverDescriptorFailures();
    std::cout << "usb recovery policy tests passed\n";
    return 0;
}
