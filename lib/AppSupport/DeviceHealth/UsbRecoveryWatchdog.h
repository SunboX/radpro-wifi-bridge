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
    enum class Action { None, WaitForTelemetry, RetryPoll, Restart };

    void recordPollService(uint32_t now)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (havePollService_ && now - lastPollServiceMs_ > longestPollGapMs_)
            longestPollGapMs_ = now - lastPollServiceMs_;
        lastPollServiceMs_ = now;
        havePollService_ = true;
    }

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
        havePollService_ = false;
        longestPollGapMs_ = 0;
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

    Action check(uint32_t now, uint32_t timeoutMs, bool updateInProgress, uint32_t observationReadyAt,
                 bool usbConnected = false)
    {
        // USB records replies on the CDC task while the main task checks the timeout.
        std::lock_guard<std::mutex> lock(mutex_);
        const bool missedPolls = havePollService_ &&
            (now - lastPollServiceMs_ >= timeoutMs || longestPollGapMs_ >= timeoutMs);
        longestPollGapMs_ = 0;
        if (!timeoutMs || updateInProgress)
        {
            armed_ = false;
            waitingForTelemetry_ = false;
            havePollService_ = false;
            return Action::None;
        }
        // A reply can arrive just after the main task sampled 'now'.
        if (!armed_ || static_cast<int32_t>(now - lastResponseMs_) < static_cast<int32_t>(timeoutMs))
            return Action::None;

        const bool justAttached = UsbAttachDelayPolicy::inOpeningGrace(now, observationReadyAt);
        if (!waitingForTelemetry_ && (justAttached || (usbConnected && missedPolls)))
        {
            waitingForTelemetry_ = true;
            graceStartedMs_ = now;
            // A blocked application can miss the entire polling window. Give a
            // connected detector one real request before deciding it is silent.
            return justAttached ? Action::WaitForTelemetry : Action::RetryPoll;
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
    bool havePollService_ = false;
    uint32_t lastPollServiceMs_ = 0;
    uint32_t longestPollGapMs_ = 0;
};
