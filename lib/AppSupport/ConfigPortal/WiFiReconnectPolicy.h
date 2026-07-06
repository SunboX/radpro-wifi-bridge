/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace WiFiReconnectPolicy
{
constexpr unsigned long kRetryIntervalMs = 5000;

enum class StationMode
{
    StationOnly,
    ApAndStation
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
} // namespace WiFiReconnectPolicy
