// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace MqttWrite
{
constexpr uint32_t deadlineMs = 250;

// PubSubClient needs a complete MQTT packet or a new TCP connection.
// Partial progress cannot renew the deadline for the rest of the packet.
template<class Send, class Now, class Wait>
int sendPacket(const uint8_t *data, size_t size, Send send, Now now, Wait wait)
{
    const uint32_t started = now();
    while (size)
    {
        if (uint32_t(now() - started) >= deadlineMs)
            return ETIMEDOUT;
        const auto sent = send(data, size);
        if (sent > 0)
        {
            data += sent;
            size -= static_cast<size_t>(sent);
            continue;
        }
        if (sent == 0)
            return EPIPE;
        const int error = errno;
        if (error != EAGAIN && error != EWOULDBLOCK && error != EINTR)
            return error;
        wait();
    }
    return 0;
}
}
