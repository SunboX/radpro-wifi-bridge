// SPDX-License-Identifier: GPL-3.0-or-later
#include "Mqtt/MqttWrite.h"
#include <cassert>
#include <climits>

int main()
{
    const uint8_t packet[]{1, 2, 3, 4};
    uint32_t time = UINT32_MAX - 40;
    auto now = [&] { return time; };
    auto wait = [&] { time += 10; };
    unsigned calls = 0;
    auto partial = [&](const uint8_t *p, size_t length) {
        switch (++calls)
        {
        case 1: assert(p == packet && length == 4); return 1;
        case 2: errno = EINTR; return -1;
        case 3: errno = EWOULDBLOCK; return -1;
        default: assert(p == packet + 1 && length == 3); return 3;
        }
    };
    assert(MqttWrite::sendPacket(packet, 4, partial, now, wait) == 0 && calls == 4);

    const auto started = time;
    auto full = [](const uint8_t *, size_t) { errno = EAGAIN; return -1; };
    assert(MqttWrite::sendPacket(packet, 4, full, now, wait) == ETIMEDOUT);
    assert(time - started == MqttWrite::deadlineMs);

    calls = 0;
    auto slowProgress = [&](const uint8_t *, size_t) { ++calls; time += 130; return 1; };
    assert(MqttWrite::sendPacket(packet, 4, slowProgress, now, wait) == ETIMEDOUT && calls == 2);
    auto closed = [](const uint8_t *, size_t) { return 0; };
    auto reset = [](const uint8_t *, size_t) { errno = ECONNRESET; return -1; };
    assert(MqttWrite::sendPacket(packet, 4, closed, now, wait) == EPIPE);
    assert(MqttWrite::sendPacket(packet, 4, reset, now, wait) == ECONNRESET);
    assert(MqttWrite::sendPacket(packet, 0, reset, now, wait) == 0);
}
