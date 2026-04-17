#pragma once

namespace UsbRecoveryPolicy
{
constexpr unsigned long kDisconnectGraceMs = 15000UL;
constexpr unsigned long kRestartIntervalMs = 30000UL;

inline bool shouldRestart(unsigned long now,
                          unsigned long disconnectedSinceMs,
                          unsigned long lastRestartAttemptMs)
{
    if (disconnectedSinceMs == 0)
        return false;

    if (static_cast<long>(now - disconnectedSinceMs) < static_cast<long>(kDisconnectGraceMs))
        return false;

    if (lastRestartAttemptMs != 0 &&
        static_cast<long>(now - lastRestartAttemptMs) < static_cast<long>(kRestartIntervalMs))
        return false;

    return true;
}
} // namespace UsbRecoveryPolicy
