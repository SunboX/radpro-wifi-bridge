#pragma once

namespace UsbAttachDelayPolicy
{
constexpr unsigned long kSettleDelayMs = 250UL;

inline unsigned long readyAt(unsigned long observedAtMs)
{
    return observedAtMs + kSettleDelayMs;
}

inline bool isReady(unsigned long nowMs, unsigned long readyAtMs)
{
    if (readyAtMs == 0)
        return true;

    return static_cast<long>(nowMs - readyAtMs) >= 0;
}
} // namespace UsbAttachDelayPolicy
