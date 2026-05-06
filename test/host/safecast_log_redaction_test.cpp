#include <cassert>
#include <iostream>
#include <string>

#include "Safecast/SafecastLogRedaction.h"

namespace
{
void testRedactsApiKeyInsideQueryString()
{
    const String url = "https://api.safecast.org/measurements.json?api_key=abcd1234wxyz&foo=bar";
    const std::string redacted = SafecastLogRedaction::redactUrlForLogs(url).c_str();

    assert(redacted.find("api_key=***REDACTED***") != std::string::npos);
    assert(redacted.find("abcd1234wxyz") == std::string::npos);
    assert(redacted.find("foo=bar") != std::string::npos);
}

void testLeavesUrlsWithoutApiKeyUntouched()
{
    const String url = "https://api.safecast.org/measurements.json?foo=bar";
    const std::string redacted = SafecastLogRedaction::redactUrlForLogs(url).c_str();

    assert(redacted == url.c_str());
}

void testMasksApiKeyForDisplay()
{
    const std::string masked = SafecastLogRedaction::maskSecretForDisplay("abcd1234wxyz").c_str();

    assert(masked == "abcd********wxyz");
}

void testReturnsConfiguredPlaceholderWhenKeyTooShort()
{
    const std::string masked = SafecastLogRedaction::maskSecretForDisplay("abc").c_str();

    assert(masked == "Configured");
}
} // namespace

int main()
{
    testRedactsApiKeyInsideQueryString();
    testLeavesUrlsWithoutApiKeyUntouched();
    testMasksApiKeyForDisplay();
    testReturnsConfiguredPlaceholderWhenKeyTooShort();
    std::cout << "safecast log redaction tests passed\n";
    return 0;
}
