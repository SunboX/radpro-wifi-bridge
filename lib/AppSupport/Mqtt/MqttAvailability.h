// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

namespace MqttAvailability
{
constexpr uint16_t kKeepAliveSeconds = 15;
constexpr uint16_t kSocketTimeoutSeconds = 3;

// Presence belongs to the bridge, even before a detector ID is known.
inline String topic(String base, String pattern, const String &bridgeId)
{
    base.trim();
    if (!base.length())
        base = "radpro/%deviceid%";
    base.replace("%deviceid%", bridgeId);
    base.replace("%DeviceId%", bridgeId);
    while (base.endsWith("/"))
        base.remove(base.length() - 1);
    if (!pattern.length())
        pattern = "%prefix%/%topic%/";
    pattern.replace("%prefix%", "stat");
    pattern.replace("%topic%", base);
    if (!pattern.endsWith("/"))
        pattern += '/';
    return pattern + "availability";
}

inline void appendDiscovery(JsonDocument &doc, const String &availabilityTopic)
{
    doc["availability_topic"] = availabilityTopic;
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";
}
}
