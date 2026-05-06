#include <cassert>
#include <iostream>
#include <string>

#include <ArduinoJson.h>

#include "Safecast/SafecastPayload.h"

namespace
{
void testBuildsMinimalPayload()
{
    SafecastPayload::Measurement measurement;
    measurement.hasLatitude = true;
    measurement.hasLongitude = true;
    measurement.hasValue = true;
    measurement.latitude = 52.520008f;
    measurement.longitude = 13.404954f;
    measurement.value = 42.5f;
    measurement.unit = "cpm";
    measurement.capturedAt = "2026-04-22T12:00:00Z";

    JsonDocument doc;
    const auto error = SafecastPayload::buildPayloadDocument(doc, measurement);

    assert(error == SafecastPayload::BuildError::None);
    assert(doc["latitude"].as<float>() == 52.520008f);
    assert(doc["longitude"].as<float>() == 13.404954f);
    assert(doc["value"].as<float>() == 42.5f);
    assert(std::string(doc["unit"].as<const char *>()) == "cpm");
    assert(std::string(doc["captured_at"].as<const char *>()) == "2026-04-22T12:00:00Z");
  }

void testBuildsPayloadWithOptionalFields()
{
    SafecastPayload::Measurement measurement;
    measurement.hasLatitude = true;
    measurement.hasLongitude = true;
    measurement.hasValue = true;
    measurement.latitude = 52.520008f;
    measurement.longitude = 13.404954f;
    measurement.value = 0.1234f;
    measurement.unit = "usv";
    measurement.capturedAt = "2026-04-22T12:00:00Z";
    measurement.deviceId = "210";
    measurement.heightCm = "100";
    measurement.locationName = "RadPro WiFi Bridge Station";

    JsonDocument doc;
    const auto error = SafecastPayload::buildPayloadDocument(doc, measurement);

    assert(error == SafecastPayload::BuildError::None);
    assert(doc["device_id"].as<int>() == 210);
    assert(doc["height"].as<int>() == 100);
    assert(std::string(doc["location_name"].as<const char *>()) == "RadPro WiFi Bridge Station");
    assert(std::string(doc["unit"].as<const char *>()) == "usv");
}

void testRejectsInvalidUnit()
{
    SafecastPayload::Measurement measurement;
    measurement.hasLatitude = true;
    measurement.hasLongitude = true;
    measurement.hasValue = true;
    measurement.latitude = 52.520008f;
    measurement.longitude = 13.404954f;
    measurement.value = 42.5f;
    measurement.unit = "uSv/h";
    measurement.capturedAt = "2026-04-22T12:00:00Z";

    JsonDocument doc;
    const auto error = SafecastPayload::buildPayloadDocument(doc, measurement);

    assert(error == SafecastPayload::BuildError::InvalidUnit);
}

void testRejectsMissingCoordinates()
{
    SafecastPayload::Measurement measurement;
    measurement.value = 42.5f;
    measurement.unit = "cpm";
    measurement.capturedAt = "2026-04-22T12:00:00Z";

    JsonDocument doc;
    const auto error = SafecastPayload::buildPayloadDocument(doc, measurement);

    assert(error == SafecastPayload::BuildError::MissingLatitude);
}
} // namespace

int main()
{
    testBuildsMinimalPayload();
    testBuildsPayloadWithOptionalFields();
    testRejectsInvalidUnit();
    testRejectsMissingCoordinates();
    std::cout << "safecast payload tests passed\n";
    return 0;
}
