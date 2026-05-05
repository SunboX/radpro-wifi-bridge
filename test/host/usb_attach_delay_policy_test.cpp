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

void testIsReadyTreatsZeroAsReady()
{
    assert(UsbAttachDelayPolicy::isReady(0, 0));
}

void testIsReadyWaitsUntilDeadline()
{
    assert(!UsbAttachDelayPolicy::isReady(1249, 1250));
    assert(UsbAttachDelayPolicy::isReady(1250, 1250));
    assert(UsbAttachDelayPolicy::isReady(1300, 1250));
}
} // namespace

int main()
{
    testReadyAtAddsSettleDelay();
    testIsReadyTreatsZeroAsReady();
    testIsReadyWaitsUntilDeadline();
    std::cout << "usb attach delay policy tests passed\n";
    return 0;
}
