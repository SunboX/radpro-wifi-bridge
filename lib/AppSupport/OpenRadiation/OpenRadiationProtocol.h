#pragma once

#include <Arduino.h>

namespace OpenRadiationProtocol
{
static constexpr const char *kSubmitHost = "submit.openradiation.net";
static constexpr const char *kRequestHost = "request.openradiation.net";
static constexpr const char *kContentType = "application/json";
static constexpr const char *kAccept = "application/json, application/vnd.api+json";

inline String resolveApparatusId(const String &configuredId, const String &deviceId)
{
    String apparatusId = configuredId;
    apparatusId.trim();
    if (apparatusId.length())
        return apparatusId;

    apparatusId = deviceId;
    apparatusId.trim();
    return apparatusId;
}

inline String buildMeasurementLookupPath(const String &reportUuid, const String &apiKey)
{
    String trimmedReportUuid = reportUuid;
    String trimmedApiKey = apiKey;
    trimmedReportUuid.trim();
    trimmedApiKey.trim();

    if (!trimmedReportUuid.length() || !trimmedApiKey.length())
        return String();

    return String("/measurements/") + trimmedReportUuid +
           "?apiKey=" + trimmedApiKey +
           "&response=complete&withEnclosedObject=no";
}

inline String buildOrganisationReporting(const String &bridgeVersion)
{
    String value = "radpro-wifi-bridge";
    if (!bridgeVersion.length())
        return value;

    value += "_";
    value += bridgeVersion;
    return value;
}
} // namespace OpenRadiationProtocol
