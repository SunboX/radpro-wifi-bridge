// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <cstdint>
#include <mutex>
#include "UsbAttachDelayPolicy.h"

// Tracks real detector responses, independently of USB attach/host restart events.
class UsbRecoveryWatchdog
{
public:
    enum class Action { None, WaitForTelemetry, Restart };

    bool recordResponse(uint32_t now)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool firstResponse = !armed_;
        armed_ = true;
        lastResponseMs_ = now;
        waitingForTelemetry_ = false;
        return firstResponse;
    }

    void reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        armed_ = false;
        waitingForTelemetry_ = false;
    }

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

    Action check(uint32_t now, uint32_t timeoutMs, bool updateInProgress, uint32_t observationReadyAt)
    {
        // USB records replies on the CDC task while the main task checks the timeout.
        std::lock_guard<std::mutex> lock(mutex_);
        if (!timeoutMs || updateInProgress)
        {
            armed_ = false;
            waitingForTelemetry_ = false;
            return Action::None;
        }
        // A reply can arrive just after the main task sampled 'now'.
        if (!armed_ || static_cast<int32_t>(now - lastResponseMs_) < static_cast<int32_t>(timeoutMs))
            return Action::None;

        if (!waitingForTelemetry_ && UsbAttachDelayPolicy::inOpeningGrace(now, observationReadyAt))
        {
            waitingForTelemetry_ = true;
            graceStartedMs_ = now;
            return Action::WaitForTelemetry;
        }
        if (waitingForTelemetry_ && now - graceStartedMs_ < UsbAttachDelayPolicy::kOpeningGraceMs)
            return Action::None;
        return Action::Restart;
    }

private:
    std::mutex mutex_;
    bool armed_ = false;
    bool waitingForTelemetry_ = false;
    uint32_t lastResponseMs_ = 0;
    uint32_t graceStartedMs_ = 0;
};
