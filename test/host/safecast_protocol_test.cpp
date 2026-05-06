#include <cassert>
#include <iostream>
#include <string>

#include "Safecast/SafecastProtocol.h"

namespace
{
void testResolvesProductionBaseUrlByDefault()
{
    const std::string url = SafecastProtocol::resolveBaseUrl(
        SafecastProtocol::kProductionApiBaseUrl,
        false,
        "").c_str();

    assert(url == "https://api.safecast.org");
}

void testResolvesTestBaseUrlWhenRequested()
{
    const std::string url = SafecastProtocol::resolveBaseUrl(
        SafecastProtocol::kProductionApiBaseUrl,
        true,
        "").c_str();

    assert(url == "https://dev.safecast.org");
}

void testCustomBaseUrlOverridesTestToggle()
{
    const std::string url = SafecastProtocol::resolveBaseUrl(
        SafecastProtocol::kProductionApiBaseUrl,
        true,
        " https://example.invalid/custom/ ");

    assert(url == "https://example.invalid/custom");
}

void testParsesHttpsBaseUrlWithPath()
{
    SafecastProtocol::Endpoint endpoint;
    const bool ok = SafecastProtocol::parseBaseUrl("https://api.safecast.org/root", endpoint);

    assert(ok);
    assert(endpoint.secure);
    assert(std::string(endpoint.host.c_str()) == "api.safecast.org");
    assert(endpoint.port == 443);
    assert(std::string(endpoint.basePath.c_str()) == "/root");
}

void testParsesHttpBaseUrlWithExplicitPort()
{
    SafecastProtocol::Endpoint endpoint;
    const bool ok = SafecastProtocol::parseBaseUrl("http://localhost:8080/api/", endpoint);

    assert(ok);
    assert(!endpoint.secure);
    assert(std::string(endpoint.host.c_str()) == "localhost");
    assert(endpoint.port == 8080);
    assert(std::string(endpoint.basePath.c_str()) == "/api");
}

void testBuildsMeasurementPathWithEscapedApiKey()
{
    SafecastProtocol::Endpoint endpoint;
    assert(SafecastProtocol::parseBaseUrl("https://api.safecast.org/root", endpoint));

    const std::string path = SafecastProtocol::buildMeasurementPath(endpoint, "abc 123").c_str();
    assert(path == "/root/measurements.json?api_key=abc%20123");
}
} // namespace

int main()
{
    testResolvesProductionBaseUrlByDefault();
    testResolvesTestBaseUrlWhenRequested();
    testCustomBaseUrlOverridesTestToggle();
    testParsesHttpsBaseUrlWithPath();
    testParsesHttpBaseUrlWithExplicitPort();
    testBuildsMeasurementPathWithEscapedApiKey();
    std::cout << "safecast protocol tests passed\n";
    return 0;
}
