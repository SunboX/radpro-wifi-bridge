// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <cstdint>

// Tracks real detector responses, independently of USB attach/host restart events.
class UsbRecoveryWatchdog
{
public:
    void recordResponse(uint32_t now)
    {
        armed_ = true;
        lastResponseMs_ = now;
    }

    void reset() { armed_ = false; }
    bool armed() const { return armed_; }

    static uint32_t effectiveTimeoutMs(uint32_t timeoutSeconds, uint32_t readIntervalMs)
    {
        if (!timeoutSeconds)
            return 0;
        // Never reboot a healthy detector just because it is polled slowly.
        const uint64_t pollGraceMs = static_cast<uint64_t>(readIntervalMs) * 3;
        if (pollGraceMs >= 0x80000000ULL)
            return 0;
        const uint32_t configuredMs = timeoutSeconds * 1000;
        return pollGraceMs > configuredMs ? static_cast<uint32_t>(pollGraceMs) : configuredMs;
    }

    bool shouldRestart(uint32_t now, uint32_t timeoutMs, bool updateInProgress)
    {
        if (!timeoutMs || updateInProgress)
        {
            reset();
            return false;
        }
        return armed_ && static_cast<uint32_t>(now - lastResponseMs_) >= timeoutMs;
    }

private:
    bool armed_ = false;
    uint32_t lastResponseMs_ = 0;
};
