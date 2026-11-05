// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "AppConfig.h"

namespace UsbRecoverySettings
{
inline bool isValidTimeout(uint32_t seconds)
{
    return seconds == 0 || (seconds >= kMinUsbRecoveryTimeoutSeconds && seconds <= kMaxUsbRecoveryTimeoutSeconds);
}

inline bool parseMinutes(String text, uint32_t &seconds)
{
    text.trim();
    if (!text.length())
        return false;
    uint32_t minutes = 0;
    for (size_t i = 0; i < text.length(); ++i)
    {
        if (text[i] < '0' || text[i] > '9')
            return false;
        minutes = minutes * 10 + static_cast<uint32_t>(text[i] - '0');
        if (minutes > kMaxUsbRecoveryTimeoutSeconds / 60)
            return false;
    }
    if (!minutes)
        return false;
    seconds = minutes * 60;
    return true;
}

inline void appendConfig(JsonDocument &doc, const AppConfig &config)
{
    doc["usbRecoveryTimeoutSeconds"] = config.usbRecoveryTimeoutSeconds;
}

inline bool applyConfig(JsonVariantConst input, AppConfig &config)
{
    const JsonVariantConst value = input["usbRecoveryTimeoutSeconds"];
    if (value.isNull())
        return true; // Older backups keep the current watchdog setting.
    if (!value.is<uint32_t>() || !isValidTimeout(value.as<uint32_t>()))
        return false;
    config.usbRecoveryTimeoutSeconds = value.as<uint32_t>();
    return true;
}
}
