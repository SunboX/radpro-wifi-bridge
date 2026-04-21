#include <cassert>
#include <iostream>
#include <string>

#include <ArduinoJson.h>

#include "AppConfig/AppConfig.h"
#include "OpenRadiation/OpenRadiationBackupJson.h"

namespace
{
void testAppConfigStorePersistsNewMeasurementFields()
{
    AppConfigStore store;
    AppConfig config;
    config.openRadiationMeasurementEnvironment = "city";
    config.openRadiationMeasurementHeight = 1.0f;
    config.openRadiationUserId = "andre";
    config.openRadiationUserPassword = "top-secret-password";
    assert(store.save(config));

    AppConfig loaded;
    assert(store.load(loaded));
    assert(std::string(loaded.openRadiationMeasurementEnvironment.c_str()) == "city");
    assert(loaded.openRadiationMeasurementHeight == 1.0f);
    assert(std::string(loaded.openRadiationUserId.c_str()) == "andre");
    assert(std::string(loaded.openRadiationUserPassword.c_str()) == "top-secret-password");
}

void testBackupJsonRoundTripsNewMeasurementFields()
{
    AppConfig config;
    config.openRadiationMeasurementEnvironment = "inside";
    config.openRadiationMeasurementHeight = 2.5f;
    config.openRadiationUserId = "andre";
    config.openRadiationUserPassword = "top-secret-password";

    JsonDocument doc;
    OpenRadiationBackupJson::appendMeasurementConfig(doc, config);
    assert(std::string(doc["openRadiationMeasurementEnvironment"].as<const char *>()) == "inside");
    assert(doc["openRadiationMeasurementHeight"].as<float>() == 2.5f);
    assert(std::string(doc["openRadiationUserId"].as<const char *>()) == "andre");
    assert(std::string(doc["openRadiationUserPassword"].as<const char *>()) == "top-secret-password");

    AppConfig updated;
    JsonDocument input;
    input["openRadiationMeasurementEnvironment"] = "city";
    input["openRadiationMeasurementHeight"] = -7.0f;
    input["openRadiationUserId"] = "andre";
    input["openRadiationUserPassword"] = "restored-secret";
    OpenRadiationBackupJson::applyMeasurementConfig(input.as<JsonVariantConst>(), updated);

    assert(std::string(updated.openRadiationMeasurementEnvironment.c_str()) == "city");
    assert(updated.openRadiationMeasurementHeight == 0.0f);
    assert(std::string(updated.openRadiationUserId.c_str()) == "andre");
    assert(std::string(updated.openRadiationUserPassword.c_str()) == "restored-secret");
}

void testBackupJsonRejectsInvalidEnvironmentAndKeepsPreviousValue()
{
    AppConfig updated;
    updated.openRadiationMeasurementEnvironment = "city";

    JsonDocument input;
    input["openRadiationMeasurementEnvironment"] = "forest";
    OpenRadiationBackupJson::applyMeasurementConfig(input.as<JsonVariantConst>(), updated);

    assert(std::string(updated.openRadiationMeasurementEnvironment.c_str()) == "city");

    input.clear();
    input["openRadiationMeasurementEnvironment"] = "";
    OpenRadiationBackupJson::applyMeasurementConfig(input.as<JsonVariantConst>(), updated);

    assert(std::string(updated.openRadiationMeasurementEnvironment.c_str()) == "city");
}
} // namespace

int main()
{
    testAppConfigStorePersistsNewMeasurementFields();
    testBackupJsonRoundTripsNewMeasurementFields();
    testBackupJsonRejectsInvalidEnvironmentAndKeepsPreviousValue();
    std::cout << "openradiation settings persistence tests passed\n";
    return 0;
}
