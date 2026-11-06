// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include "Mqtt/MqttCommandQueue.h"

int main()
{
    MqttCommandQueue queue;
    using Type = DeviceManager::CommandType;
    queue.push(Type::DeviceId, "detector-id");
    queue.push(Type::TubeRate, "12.5");
    // A blocked network request must not produce an unbounded telemetry backlog.
    for (int i = 0; i < 10000; ++i)
        queue.push(Type::TubeRate, String(i));
    queue.push(Type::DeviceBatteryVoltage, "4.1");
    MqttCommandQueue::Result result;
    assert(queue.pop(result) && result.type == Type::DeviceId && result.value == "detector-id");
    assert(queue.pop(result) && result.type == Type::TubeRate && result.value == "9999");
    assert(queue.pop(result) && result.type == Type::DeviceBatteryVoltage && result.value == "4.1");
    assert(!queue.pop(result));
    queue.push(Type::TubeRate, "13.0");
    assert(queue.pop(result) && result.value == "13.0");
    assert(!queue.pop(result));
    std::cout << "MQTT response queue coalescing and ordering tests passed\n";
}
