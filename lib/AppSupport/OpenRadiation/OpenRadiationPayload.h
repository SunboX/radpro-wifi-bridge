#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "AppConfig/AppConfig.h"
#include "OpenRadiation/OpenRadiationMeasurementMetadata.h"

namespace OpenRadiationPayload
{
static constexpr const char *kRedactedSecret = "***REDACTED***";

enum class BuildError
{
    None,
    MissingApparatusId,
    MissingApiKey,
    MissingUserId,
    MissingUserPassword,
    MissingStartTime,
};

inline const char *buildErrorText(BuildError error)
{
    switch (error)
    {
    case BuildError::None:
        return "";
    case BuildError::MissingApparatusId:
        return "missing required OpenRadiation apparatus ID";
    case BuildError::MissingApiKey:
        return "missing required OpenRadiation credentials: API key";
    case BuildError::MissingUserId:
        return "missing required OpenRadiation credentials: user ID";
    case BuildError::MissingUserPassword:
        return "missing required OpenRadiation credentials: user password";
    case BuildError::MissingStartTime:
        return "missing required OpenRadiation startTime";
    }
    return "unknown OpenRadiation payload error";
}

inline String buildRequiredCredentialError(const AppConfig &config)
{
    String missing;

    auto appendMissing = [&missing](const char *label) {
        if (missing.length())
            missing += ", ";
        missing += label;
    };

    if (!config.openRadiationApiKey.length())
        appendMissing("API key");
    if (!config.openRadiationUserId.length())
        appendMissing("user ID");
    if (!config.openRadiationUserPassword.length())
        appendMissing("user password");

    if (!missing.length())
        return String();

    return String("missing required OpenRadiation credentials: ") + missing;
}

inline BuildError buildPayloadDocument(JsonDocument &doc,
                                       const AppConfig &config,
                                       const String &apparatusId,
                                       const String &apparatusVersion,
                                       const String &organisationReporting,
                                       const String &reportUuid,
                                       float doseRate,
                                       const String &startTime,
                                       const String &endTime,
                                       const String &startPulseCount,
                                       const String &endPulseCount)
{
    if (!apparatusId.length())
        return BuildError::MissingApparatusId;
    if (!config.openRadiationApiKey.length())
        return BuildError::MissingApiKey;
    if (!config.openRadiationUserId.length())
        return BuildError::MissingUserId;
    if (!config.openRadiationUserPassword.length())
        return BuildError::MissingUserPassword;
    if (!startTime.length())
        return BuildError::MissingStartTime;

    doc.clear();
    doc["apiKey"] = config.openRadiationApiKey;

    JsonObject data = doc["data"].to<JsonObject>();
    data["reportUuid"] = reportUuid;
    data["apparatusId"] = apparatusId;
    data["value"] = doseRate;
    data["startTime"] = startTime;
    data["latitude"] = config.openRadiationLatitude;
    data["longitude"] = config.openRadiationLongitude;
    if (config.openRadiationAltitude != 0.0f)
        data["altitude"] = lroundf(config.openRadiationAltitude);
    if (config.openRadiationAccuracy > 0.0f)
        data["accuracy"] = config.openRadiationAccuracy;
    if (apparatusVersion.length())
        data["apparatusVersion"] = apparatusVersion;
    data["apparatusSensorType"] = "geiger";
    data["manualReporting"] = false;
    data["organisationReporting"] = organisationReporting;
    data["reportContext"] = "routine";
    data["userId"] = config.openRadiationUserId;
    data["userPwd"] = config.openRadiationUserPassword;

    OpenRadiationMeasurementMetadata::appendOptionalFields(
        data,
        config,
        endTime,
        startPulseCount,
        endPulseCount);

    return BuildError::None;
}

inline void redactSecrets(JsonDocument &doc)
{
    if (!doc["apiKey"].isNull())
        doc["apiKey"] = kRedactedSecret;

    JsonVariant data = doc["data"];
    if (!data.isNull() && !data["userPwd"].isNull())
        data["userPwd"] = kRedactedSecret;
}
} // namespace OpenRadiationPayload
