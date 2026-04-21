#include <cassert>
#include <iostream>
#include <string>

#include <ArduinoJson.h>

#include "AppConfig/AppConfig.h"
#include "OpenRadiation/OpenRadiationPayload.h"

using OpenRadiationPayload::BuildError;
using OpenRadiationPayload::buildPayloadDocument;
using OpenRadiationPayload::buildRequiredCredentialError;
using OpenRadiationPayload::redactSecrets;

namespace
{
AppConfig makeConfig()
{
    AppConfig config;
    config.openRadiationApiKey = "api-key-123";
    config.openRadiationUserId = "andre";
    config.openRadiationUserPassword = "top-secret-password";
    config.openRadiationLatitude = 50.497f;
    config.openRadiationLongitude = 12.137f;
    config.openRadiationAltitude = 350.0f;
    config.openRadiationAccuracy = 17.5f;
    config.openRadiationMeasurementEnvironment = "city";
    config.openRadiationMeasurementHeight = 1.0f;
    return config;
}

void testBuildsPayloadWithUserAssociationAndRoutineContext()
{
    AppConfig config = makeConfig();

    JsonDocument doc;
    const BuildError error = buildPayloadDocument(doc,
                                                  config,
                                                  "andre-plauen-fixed-beacon",
                                                  "1.0",
                                                  "radpro-wifi-bridge_1.15.2",
                                                  "3d72f4fb-8f7e-4c25-8f1d-f3b3f5dbe001",
                                                  0.12f,
                                                  "2026-04-21T12:00:00Z",
                                                  "2026-04-21T12:05:00Z",
                                                  "100",
                                                  "142");
    assert(error == BuildError::None);

    assert(std::string(doc["apiKey"].as<const char *>()) == "api-key-123");

    JsonObject data = doc["data"].as<JsonObject>();
    assert(std::string(data["reportUuid"].as<const char *>()) == "3d72f4fb-8f7e-4c25-8f1d-f3b3f5dbe001");
    assert(std::string(data["apparatusId"].as<const char *>()) == "andre-plauen-fixed-beacon");
    assert(std::string(data["apparatusVersion"].as<const char *>()) == "1.0");
    assert(std::string(data["organisationReporting"].as<const char *>()) == "radpro-wifi-bridge_1.15.2");
    assert(std::string(data["reportContext"].as<const char *>()) == "routine");
    assert(data["manualReporting"].as<bool>() == false);
    assert(std::string(data["userId"].as<const char *>()) == "andre");
    assert(std::string(data["userPwd"].as<const char *>()) == "top-secret-password");
    assert(std::string(data["measurementEnvironment"].as<const char *>()) == "city");
    assert(data["measurementHeight"].as<float>() == 1.0f);
    assert(data["hitsNumber"].as<uint32_t>() == 42);
}

void testRequiresApiKeyUserIdAndUserPassword()
{
    AppConfig config = makeConfig();
    JsonDocument doc;

    config.openRadiationApiKey = "";
    assert(buildPayloadDocument(doc,
                                config,
                                "apparatus",
                                "1.0",
                                "radpro-wifi-bridge_1.15.2",
                                "uuid",
                                0.12f,
                                "2026-04-21T12:00:00Z",
                                "2026-04-21T12:05:00Z",
                                "100",
                                "142") == BuildError::MissingApiKey);

    config = makeConfig();
    config.openRadiationUserId = "";
    assert(buildPayloadDocument(doc,
                                config,
                                "apparatus",
                                "1.0",
                                "radpro-wifi-bridge_1.15.2",
                                "uuid",
                                0.12f,
                                "2026-04-21T12:00:00Z",
                                "2026-04-21T12:05:00Z",
                                "100",
                                "142") == BuildError::MissingUserId);

    config = makeConfig();
    config.openRadiationUserPassword = "";
    assert(buildPayloadDocument(doc,
                                config,
                                "apparatus",
                                "1.0",
                                "radpro-wifi-bridge_1.15.2",
                                "uuid",
                                0.12f,
                                "2026-04-21T12:00:00Z",
                                "2026-04-21T12:05:00Z",
                                "100",
                                "142") == BuildError::MissingUserPassword);

    config = makeConfig();
    config.openRadiationUserPassword = "";
    const std::string message = buildRequiredCredentialError(config).c_str();
    assert(message.find("API key") == std::string::npos);
    assert(message.find("user ID") == std::string::npos);
    assert(message.find("user password") != std::string::npos);
    assert(message.find("top-secret-password") == std::string::npos);
}

void testRedactsApiKeyAndUserPasswordButKeepsUserId()
{
    AppConfig config = makeConfig();
    JsonDocument doc;

    const BuildError error = buildPayloadDocument(doc,
                                                  config,
                                                  "apparatus",
                                                  "1.0",
                                                  "radpro-wifi-bridge_1.15.2",
                                                  "uuid",
                                                  0.12f,
                                                  "2026-04-21T12:00:00Z",
                                                  "2026-04-21T12:05:00Z",
                                                  "100",
                                                  "142");
    assert(error == BuildError::None);

    redactSecrets(doc);

    assert(std::string(doc["apiKey"].as<const char *>()) == "***REDACTED***");
    assert(std::string(doc["data"]["userId"].as<const char *>()) == "andre");
    assert(std::string(doc["data"]["userPwd"].as<const char *>()) == "***REDACTED***");
}
} // namespace

int main()
{
    testBuildsPayloadWithUserAssociationAndRoutineContext();
    testRequiresApiKeyUserIdAndUserPassword();
    testRedactsApiKeyAndUserPasswordButKeepsUserId();
    std::cout << "openradiation payload tests passed\n";
    return 0;
}
