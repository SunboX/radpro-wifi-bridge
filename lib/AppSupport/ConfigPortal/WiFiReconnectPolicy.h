/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace WiFiReconnectPolicy
{
constexpr unsigned long kRetryIntervalMs = 5000;
constexpr unsigned long kDhcpTimeoutMs = 10000;
constexpr unsigned long kGatewayProbeIntervalMs = 30000;
constexpr uint8_t kGatewayFailureThreshold = 3;

enum class StationMode
{
    StationOnly,
    ApAndStation
};

struct GatewayHealthState
{
    bool provenReachable = false;
    uint8_t consecutiveFailures = 0;
};

enum class GatewayProbeOutcome
{
    IgnoredUntilProven,
    Armed,
    Healthy,
    Suspect,
    Stale
};

inline bool shouldSuppressRetry(bool configPortalActive, bool onboarding, bool haveStoredCredentials)
{
    return onboarding || (configPortalActive && !haveStoredCredentials);
}

inline bool isRetryDue(unsigned long now, unsigned long lastAttemptMs, unsigned long intervalMs = kRetryIntervalMs)
{
    return lastAttemptMs == 0 || now - lastAttemptMs >= intervalMs;
}

inline StationMode reconnectMode(bool configPortalActive)
{
    return configPortalActive ? StationMode::ApAndStation : StationMode::StationOnly;
}

inline bool isConnectionAttemptInFlight(unsigned long now,
                                        unsigned long startedAtMs,
                                        unsigned long timeoutMs = kDhcpTimeoutMs)
{
    return startedAtMs != 0 && now - startedAtMs < timeoutMs;
}

inline unsigned long lastAttemptAfterDisconnect(bool alreadyPending, unsigned long lastAttemptMs)
{
    return alreadyPending ? lastAttemptMs : 0;
}

inline GatewayProbeOutcome recordGatewayProbe(GatewayHealthState &state,
                                              bool succeeded,
                                              uint8_t failureThreshold = kGatewayFailureThreshold)
{
    if (succeeded)
    {
        const bool wasProven = state.provenReachable;
        state.provenReachable = true;
        state.consecutiveFailures = 0;
        return wasProven ? GatewayProbeOutcome::Healthy : GatewayProbeOutcome::Armed;
    }

    if (!state.provenReachable)
    {
        return GatewayProbeOutcome::IgnoredUntilProven;
    }

    if (state.consecutiveFailures < UINT8_MAX)
    {
        ++state.consecutiveFailures;
    }

    return state.consecutiveFailures >= failureThreshold ? GatewayProbeOutcome::Stale
                                                          : GatewayProbeOutcome::Suspect;
}

inline void resetGatewayHealth(GatewayHealthState &state)
{
    state = GatewayHealthState{};
}
} // namespace WiFiReconnectPolicy
