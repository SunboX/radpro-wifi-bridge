// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include <string>

#include <ArduinoJson.h>

#include "OpenRadiation/OpenRadiationMeasurementMetadata.h"

using OpenRadiationMeasurementMetadata::appendOptionalFields;
using OpenRadiationMeasurementMetadata::clampMeasurementHeight;
using OpenRadiationMeasurementMetadata::isValidMeasurementEnvironment;
using OpenRadiationMeasurementMetadata::tryComputeHitsNumber;

namespace
{
void testAcceptsKnownMeasurementEnvironmentValues()
{
    assert(isValidMeasurementEnvironment("city"));
    assert(isValidMeasurementEnvironment("countryside"));
    assert(isValidMeasurementEnvironment("ontheroad"));
    assert(isValidMeasurementEnvironment("inside"));
    assert(isValidMeasurementEnvironment("plane"));
}

void testRejectsUnknownMeasurementEnvironmentValues()
{
    assert(!isValidMeasurementEnvironment(""));
    assert(!isValidMeasurementEnvironment("forest"));
    assert(!isValidMeasurementEnvironment("City"));
}

void testClampsMeasurementHeightToSupportedRange()
{
    assert(clampMeasurementHeight(-1.0f) == 0.0f);
    assert(clampMeasurementHeight(1.0f) == 1.0f);
    assert(clampMeasurementHeight(120.0f) == 100.0f);
}

void testComputesHitsNumberFromPulseDelta()
{
    uint32_t hits = 0;
    assert(tryComputeHitsNumber("100", "142", hits));
    assert(hits == 42);
    assert(!tryComputeHitsNumber("150", "142", hits));
    assert(!tryComputeHitsNumber("abc", "142", hits));
    assert(!tryComputeHitsNumber("-2", "-1", hits));
    assert(!tryComputeHitsNumber("4294967296", "4294967297", hits));
}

void testAppendsOptionalFieldsToPayload()
{
    AppConfig config;
    config.openRadiationMeasurementEnvironment = "city";
    config.openRadiationMeasurementHeight = 1.0f;
    config.openRadiationAccuracy = 17.5f;

    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();
    appendOptionalFields(data, config, "2026-04-17T18:11:00Z", "100", "142");

    assert(std::string(data["measurementEnvironment"].as<const char *>()) == "city");
    assert(data["measurementHeight"].as<float>() == 1.0f);
    assert(data["altitudeAccuracy"].as<float>() == 17.5f);
    assert(std::string(data["endTime"].as<const char *>()) == "2026-04-17T18:11:00Z");
    assert(data["hitsNumber"].as<uint32_t>() == 42);
}

void testOmitsInvalidOptionalFieldsFromPayload()
{
    AppConfig config;
    config.openRadiationMeasurementEnvironment = "forest";
    config.openRadiationMeasurementHeight = 0.0f;
    config.openRadiationAccuracy = -1.0f;

    JsonDocument doc;
    JsonObject data = doc.to<JsonObject>();
    appendOptionalFields(data, config, "", "100", "142");

    assert(data["measurementEnvironment"].isNull());
    assert(data["measurementHeight"].isNull());
    assert(data["altitudeAccuracy"].isNull());
    assert(data["endTime"].isNull());
    assert(data["hitsNumber"].as<uint32_t>() == 42);
}
} // namespace

int main()
{
    testAcceptsKnownMeasurementEnvironmentValues();
    testRejectsUnknownMeasurementEnvironmentValues();
    testClampsMeasurementHeightToSupportedRange();
    testComputesHitsNumberFromPulseDelta();
    testAppendsOptionalFieldsToPayload();
    testOmitsInvalidOptionalFieldsFromPayload();
    std::cout << "openradiation measurement metadata tests passed\n";
    return 0;
}
