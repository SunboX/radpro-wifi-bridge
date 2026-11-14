/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace OpenSenseMapBackoff
{
// Leave headroom below the API limit, including other clients on the same IP.
constexpr unsigned long kMinPublishGapMs = 15000;

inline unsigned long retryDelayMs(unsigned failures, int httpStatusCode)
{
    constexpr unsigned long kMaxBackoffMs = 60000;
    if (httpStatusCode == 429)
        return kMaxBackoffMs;

    unsigned long backoff = 10000;
    for (unsigned i = 1; i < failures && backoff < kMaxBackoffMs; ++i)
    {
        backoff *= 2;
        if (backoff > kMaxBackoffMs)
            backoff = kMaxBackoffMs;
    }
    return backoff;
}

inline unsigned long preserveActiveSuppression(unsigned long now, unsigned long suppressUntilMs)
{
    if (suppressUntilMs != 0 && static_cast<long>(now - suppressUntilMs) < 0)
        return suppressUntilMs;
    return 0;
}
} // namespace OpenSenseMapBackoff
