// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include "Mqtt/MqttAvailability.h"

int main()
{
    const String bridgeId = "esp32s3-aabbccddeeff";
    const String defaultTopic = MqttAvailability::topic("", "", bridgeId);
    assert(defaultTopic == "stat/radpro/esp32s3-aabbccddeeff/availability");
    assert(MqttAvailability::topic(" radpro/%deviceid%/// ", "%prefix%/%topic%/", bridgeId) == defaultTopic);
    assert(MqttAvailability::topic("counter/%DeviceId%", "home/%topic%/%prefix%", bridgeId) ==
           "home/counter/esp32s3-aabbccddeeff/stat/availability");
    JsonDocument doc;
    doc["unique_id"] = "radpro_detector_tube_rate";
    doc["state_topic"] = "stat/radpro/detector/tubeRate";
    MqttAvailability::appendDiscovery(doc, defaultTopic);
    assert(doc["availability_topic"].as<std::string>() == std::string(defaultTopic));
    assert(doc["payload_available"].as<std::string>() == "online");
    assert(doc["payload_not_available"].as<std::string>() == "offline");
    assert(doc["unique_id"].as<std::string>() == "radpro_detector_tube_rate");
    assert(doc["state_topic"].as<std::string>() == "stat/radpro/detector/tubeRate");
    std::cout << "MQTT availability topic and discovery tests passed\n";
}
