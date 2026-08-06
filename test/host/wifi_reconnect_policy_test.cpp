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

void testConnectionAttemptWaitsForDhcp()
{
    assert(WiFiReconnectPolicy::isConnectionAttemptInFlight(10999, 1000));
    assert(!WiFiReconnectPolicy::isConnectionAttemptInFlight(11000, 1000));
    assert(!WiFiReconnectPolicy::isConnectionAttemptInFlight(1000, 0));
}

void testRepeatedDisconnectPreservesRetryDelay()
{
    assert(WiFiReconnectPolicy::lastAttemptAfterDisconnect(false, 4321) == 0);
    assert(WiFiReconnectPolicy::lastAttemptAfterDisconnect(true, 4321) == 4321);
}

void testUnprovenGatewayFailuresNeverForceRecovery()
{
    WiFiReconnectPolicy::GatewayHealthState state;
    for (int i = 0; i < 10; ++i)
    {
        assert(WiFiReconnectPolicy::recordGatewayProbe(state, false) ==
               WiFiReconnectPolicy::GatewayProbeOutcome::IgnoredUntilProven);
    }
    assert(!state.provenReachable);
    assert(state.consecutiveFailures == 0);
}

void testGatewayBecomesStaleAfterThreeProvenFailures()
{
    WiFiReconnectPolicy::GatewayHealthState state;
    assert(WiFiReconnectPolicy::recordGatewayProbe(state, true) ==
           WiFiReconnectPolicy::GatewayProbeOutcome::Armed);
    assert(state.provenReachable);
    assert(WiFiReconnectPolicy::recordGatewayProbe(state, false) ==
           WiFiReconnectPolicy::GatewayProbeOutcome::Suspect);
    assert(WiFiReconnectPolicy::recordGatewayProbe(state, false) ==
           WiFiReconnectPolicy::GatewayProbeOutcome::Suspect);
    assert(WiFiReconnectPolicy::recordGatewayProbe(state, false) ==
           WiFiReconnectPolicy::GatewayProbeOutcome::Stale);
}

void testGatewaySuccessClearsFailureStreak()
{
    WiFiReconnectPolicy::GatewayHealthState state;
    WiFiReconnectPolicy::recordGatewayProbe(state, true);
    WiFiReconnectPolicy::recordGatewayProbe(state, false);
    assert(WiFiReconnectPolicy::recordGatewayProbe(state, true) ==
           WiFiReconnectPolicy::GatewayProbeOutcome::Healthy);
    assert(state.consecutiveFailures == 0);
}

void testNewLeaseResetsGatewayProof()
{
    WiFiReconnectPolicy::GatewayHealthState state;
    WiFiReconnectPolicy::recordGatewayProbe(state, true);
    WiFiReconnectPolicy::recordGatewayProbe(state, false);
    WiFiReconnectPolicy::resetGatewayHealth(state);
    assert(!state.provenReachable);
    assert(state.consecutiveFailures == 0);
}
} // namespace

int main()
{
    testStoredCredentialsKeepRetryActiveDuringConfigPortal();
    testOnboardingSuppressesRetry();
    testRetryInterval();
    testPortalReconnectKeepsApOnline();
    testConnectionAttemptWaitsForDhcp();
    testRepeatedDisconnectPreservesRetryDelay();
    testUnprovenGatewayFailuresNeverForceRecovery();
    testGatewayBecomesStaleAfterThreeProvenFailures();
    testGatewaySuccessClearsFailureStreak();
    testNewLeaseResetsGatewayProof();
    std::cout << "wifi reconnect policy tests passed\n";
    return 0;
}
