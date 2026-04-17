#include <cassert>
#include <iostream>
#include <string>

#include "OpenRadiation/OpenRadiationProtocol.h"

namespace
{
void testUsesLiveSubmitHostAndJsonMediaType()
{
    assert(std::string(OpenRadiationProtocol::kSubmitHost) == "submit.openradiation.net");
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
} // namespace

int main()
{
    testUsesLiveSubmitHostAndJsonMediaType();
    testConfiguredApparatusIdWins();
    testDeviceIdFallbackIsUsedWhenConfigMissing();
    std::cout << "openradiation protocol tests passed\n";
    return 0;
}
