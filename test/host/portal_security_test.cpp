#include <cassert>
#include <vector>

#include <Arduino.h>
#include "ConfigPortal/PortalSecurity.h"

namespace
{
    void testCsrfTokenValidation()
    {
        assert(!PortalSecurity::isValidCsrfToken("", "abc123"));
        assert(!PortalSecurity::isValidCsrfToken("abc123", ""));
        assert(!PortalSecurity::isValidCsrfToken("abc124", "abc123"));
        assert(PortalSecurity::isValidCsrfToken("abc123", "abc123"));
    }

    void testMissingFieldReport()
    {
        std::vector<const char *> expected = {
            "csrf",
            "mqttHost",
            "mqttPort",
            "mqttClient"};
        std::vector<String> present = {
            "csrf",
            "mqttPort"};

        const std::vector<String> missing = PortalSecurity::missingRequiredFields(expected, present);
        assert(missing.size() == 2);
        assert(missing[0] == "mqttHost");
        assert(missing[1] == "mqttClient");
        assert(PortalSecurity::joinFieldNames(missing) == "mqttHost,mqttClient");
    }

    void testChangedFieldNamesDoNotIncludeValues()
    {
        std::vector<String> changed;
        PortalSecurity::appendChangedField(changed, "mqttPassword", true);
        PortalSecurity::appendChangedField(changed, "mqttHost", false);
        PortalSecurity::appendChangedField(changed, "mqttTopic", true);

        assert(changed.size() == 2);
        assert(PortalSecurity::joinFieldNames(changed) == "mqttPassword,mqttTopic");
    }
}

int main()
{
    testCsrfTokenValidation();
    testMissingFieldReport();
    testChangedFieldNamesDoNotIncludeValues();
    return 0;
}
