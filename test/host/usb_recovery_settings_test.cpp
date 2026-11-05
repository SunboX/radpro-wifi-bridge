// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include "AppConfig/UsbRecoverySettings.h"

void testDefaultsAndPersistence()
{
    AppConfig config;
    assert(config.usbRecoveryTimeoutSeconds == 300);
    AppConfigStore store;
    for (uint32_t timeout : {0u, 60u, 300u, 3600u})
    {
        config.usbRecoveryTimeoutSeconds = timeout;
        assert(store.save(config));
        AppConfig restored;
        assert(store.load(restored));
        assert(restored.usbRecoveryTimeoutSeconds == timeout);
    }
    for (uint32_t invalid : {1u, 59u, 3601u, UINT32_MAX})
    {
        config.usbRecoveryTimeoutSeconds = invalid;
        assert(store.save(config));
        AppConfig restored;
        assert(store.load(restored));
        assert(restored.usbRecoveryTimeoutSeconds == 300);
    }
}
void testStrictMinuteInput()
{
    uint32_t seconds = 999;
    assert(UsbRecoverySettings::parseMinutes(" 5 ", seconds) && seconds == 300);
    assert(UsbRecoverySettings::parseMinutes("1", seconds) && seconds == 60);
    assert(UsbRecoverySettings::parseMinutes("60", seconds) && seconds == 3600);
    for (const char *text : {"", "0", "61", "-1", "5x", "1.5", "+5", "4294967296000"})
        assert(!UsbRecoverySettings::parseMinutes(text, seconds));
}
void testBackupRoundTripAndValidation()
{
    AppConfig config;
    config.usbRecoveryTimeoutSeconds = 600;
    JsonDocument doc;
    UsbRecoverySettings::appendConfig(doc, config);
    AppConfig restored;
    assert(UsbRecoverySettings::applyConfig(doc.as<JsonVariantConst>(), restored));
    assert(restored.usbRecoveryTimeoutSeconds == 600);
    JsonDocument oldBackup;
    assert(UsbRecoverySettings::applyConfig(oldBackup.as<JsonVariantConst>(), restored));
    assert(restored.usbRecoveryTimeoutSeconds == 600);
    for (int invalid : {-1, 1, 59, 3601})
    {
        doc["usbRecoveryTimeoutSeconds"] = invalid;
        assert(!UsbRecoverySettings::applyConfig(doc.as<JsonVariantConst>(), restored));
        assert(restored.usbRecoveryTimeoutSeconds == 600);
    }
    doc["usbRecoveryTimeoutSeconds"] = "300";
    assert(!UsbRecoverySettings::applyConfig(doc.as<JsonVariantConst>(), restored));
    doc["usbRecoveryTimeoutSeconds"] = 0;
    assert(UsbRecoverySettings::applyConfig(doc.as<JsonVariantConst>(), restored));
    assert(restored.usbRecoveryTimeoutSeconds == 0);
}
int main()
{
    testDefaultsAndPersistence();
    testStrictMinuteInput();
    testBackupRoundTripAndValidation();
    std::cout << "USB recovery settings tests passed\n";
}
