// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace RadProHttp
{
// One budget for the request line and all headers. The body has its own reader.
class HeaderReader
{
public:
    static constexpr uint32_t kTimeoutMs = 3000;
    static constexpr size_t kMaxBytes = 8192;

    explicit HeaderReader(uint32_t started) : started_(started) {}

    template <typename Client, typename Line, typename Clock, typename Idle>
    bool readLine(Client &client, Line &line, Clock clock, Idle idle)
    {
        line = "";
        bool carriageReturn = false;
        for (;;)
        {
            if (static_cast<uint32_t>(clock() - started_) >= kTimeoutMs || bytes_ >= kMaxBytes)
                return false;
            const int value = client.read();
            if (value < 0)
            {
                idle(); // Block for a tick so the CPU's idle task can feed its watchdog.
                continue;
            }
            ++bytes_;
            if (carriageReturn)
                return value == '\n';
            if (value == '\r')
                carriageReturn = true;
            else if (value == '\n' || value == 0)
                return false;
            else
            {
                const size_t previous = line.length();
                line += static_cast<char>(value);
                if (line.length() != previous + 1)
                    return false;
            }
            if (bytes_ % 128 == 0)
                idle();
        }
    }

private:
    uint32_t started_;
    size_t bytes_ = 0;
};
}
