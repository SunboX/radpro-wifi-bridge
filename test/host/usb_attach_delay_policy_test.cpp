// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>

#include "UsbAttachDelayPolicy.h"

namespace
{
void testReadyAtAddsSettleDelay()
{
    assert(UsbAttachDelayPolicy::readyAt(1000) == 1250);
}

void testSettleWaitUsesOnlyTimeLeftInThisWindow()
{
    using UsbAttachDelayPolicy::remainingDelay;
    assert(remainingDelay(1000, 1250, 250) == 250);
    assert(remainingDelay(1249, 1250, 250) == 1);
    assert(remainingDelay(1250, 1250, 250) == 0);
    assert(remainingDelay(1300, 1250, 250) == 0);
    assert(remainingDelay(1000, 0, 250) == 0);
    assert(remainingDelay(UINT32_MAX - 19, 20, 40) == 40);
    assert(remainingDelay(UINT32_MAX - 19, 0, 40) == 0);
    assert(remainingDelay(0x90000000, 1250, 250) == 0);
}
} // namespace

int main()
{
    testReadyAtAddsSettleDelay();
    testSettleWaitUsesOnlyTimeLeftInThisWindow();
    std::cout << "usb attach delay policy tests passed\n";
    return 0;
}
