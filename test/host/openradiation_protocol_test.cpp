#include <cassert>
#include <iostream>
#include <string>

#include "OpenRadiation/OpenRadiationProtocol.h"

namespace
{
void testUsesLiveSubmitHostAndJsonMediaType()
{
    assert(std::string(OpenRadiationProtocol::kSubmitHost) == "submit.openradiation.net");
    assert(std::string(OpenRadiationProtocol::kRequestHost) == "request.openradiation.net");
    assert(std::string(OpenRadiationProtocol::kContentType) == "application/json");
}

void testConfiguredApparatusIdWins()
{
    const String configured = OpenRadiationProtocol::resolveApparatusId("custom-sensor-id", "device-123");
    assert(std::string(configured.c_str()) == "custom-sensor-id");
}

void testDeviceIdFallbackIsUsedWhenConfigMissing()
{
    const String fallback = OpenRadiationProtocol::resolveApparatusId("", "device-123");
    assert(std::string(fallback.c_str()) == "device-123");
}

void testBuildsMeasurementLookupPathFromTrimmedValues()
{
    const String path = OpenRadiationProtocol::buildMeasurementLookupPath(
        "  bbf2ff7c-83f8-4d62-a5ab-900e216bb170  ",
        "  api-key-123  ");
    assert(std::string(path.c_str()) ==
           "/measurements/bbf2ff7c-83f8-4d62-a5ab-900e216bb170?apiKey=api-key-123&response=complete&withEnclosedObject=no");
}

void testReturnsEmptyLookupPathWhenInputsMissing()
{
    assert(std::string(OpenRadiationProtocol::buildMeasurementLookupPath("", "api-key").c_str()).empty());
    assert(std::string(OpenRadiationProtocol::buildMeasurementLookupPath("uuid", "").c_str()).empty());
}
} // namespace

int main()
{
    testUsesLiveSubmitHostAndJsonMediaType();
    testConfiguredApparatusIdWins();
    testDeviceIdFallbackIsUsedWhenConfigMissing();
    testBuildsMeasurementLookupPathFromTrimmedValues();
    testReturnsEmptyLookupPathWhenInputsMissing();
    std::cout << "openradiation protocol tests passed\n";
    return 0;
}
