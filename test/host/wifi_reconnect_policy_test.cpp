// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>

#include "ConfigPortal/WiFiReconnectPolicy.h"

namespace
{
void testStoredCredentialsKeepRetryActiveDuringConfigPortal()
{
    assert(!WiFiReconnectPolicy::shouldSuppressRetry(true, false, true));
}

void testOnboardingSuppressesRetry()
{
    assert(WiFiReconnectPolicy::shouldSuppressRetry(true, true, false));
}

void testRetryInterval()
{
    assert(WiFiReconnectPolicy::isRetryDue(1000, 0));
    assert(!WiFiReconnectPolicy::isRetryDue(5999, 1000));
    assert(WiFiReconnectPolicy::isRetryDue(6000, 1000));
}

void testPortalReconnectKeepsApOnline()
{
    assert(WiFiReconnectPolicy::reconnectMode(true) == WiFiReconnectPolicy::StationMode::ApAndStation);
    assert(WiFiReconnectPolicy::reconnectMode(false) == WiFiReconnectPolicy::StationMode::StationOnly);
}
} // namespace

int main()
{
    testStoredCredentialsKeepRetryActiveDuringConfigPortal();
    testOnboardingSuppressesRetry();
    testRetryInterval();
    testPortalReconnectKeepsApOnline();
    std::cout << "wifi reconnect policy tests passed\n";
    return 0;
}
