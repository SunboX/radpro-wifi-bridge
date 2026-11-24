/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <cstdint>

namespace UsbAttachDelayPolicy
{
constexpr unsigned long kSettleDelayMs = 250UL;
constexpr uint32_t kOpeningGraceMs = 15000;

inline bool inOpeningGrace(uint32_t now, uint32_t readyAt)
{
    // readyAt can still be one settle interval in the future.
    const int32_t sinceReady = static_cast<int32_t>(now - readyAt);
    return readyAt != 0 && sinceReady >= -static_cast<int32_t>(kSettleDelayMs) &&
           sinceReady < static_cast<int32_t>(kOpeningGraceMs);
}

inline unsigned long readyAt(unsigned long observedAtMs)
{
    return observedAtMs + kSettleDelayMs;
}

inline uint32_t remainingDelay(uint32_t now, uint32_t readyAt, uint32_t maximumDelay)
{
    const uint32_t remaining = readyAt - now;
    return readyAt != 0 && remaining <= maximumDelay ? remaining : 0;
}
} // namespace UsbAttachDelayPolicy
