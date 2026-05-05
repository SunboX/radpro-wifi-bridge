/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace OpenSenseMapTls
{
inline int normalizeMbedTlsErrorCode(int code)
{
    return code < 0 ? code : 0;
}

inline bool isCtrDrbgInputTooLarge(int code)
{
    return code == -0x0038;
}

template <typename ClientLike, typename ClockFn, typename IdleFn>
bool waitForResponse(ClientLike &client,
                     unsigned long timeoutMs,
                     ClockFn nowFn,
                     IdleFn idleFn)
{
    const unsigned long start = nowFn();
    while (true)
    {
        int available = client.available();
        if (available > 0)
            return true;
        if (available < 0)
            return false;
        if (static_cast<long>(nowFn() - start) >= static_cast<long>(timeoutMs))
            return false;
        idleFn();
    }
}
} // namespace OpenSenseMapTls
