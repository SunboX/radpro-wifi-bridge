#include <cassert>
#include <iostream>
#include <string>

#include <ArduinoJson.h>

#include "AppConfig/AppConfig.h"
#include "Safecast/SafecastBackupJson.h"

namespace
{
void testAppConfigStorePersistsSafecastFields()
{
    AppConfigStore store;
    AppConfig config;
    config.safecastEnabled = true;
    config.safecastApiBaseUrl = "https://api.safecast.org";
    config.safecastUseTestApi = true;
    config.safecastCustomApiBaseUrl = "https://example.invalid";
    config.safecastApiKey = "secret-token";
    config.safecastDeviceId = "210";
    config.safecastLatitude = "52.520008";
    config.safecastLongitude = "13.404954";
    config.safecastHeightCm = "100";
    config.safecastLocationName = "Berlin test station";
    config.safecastUnit = "usv";
    config.safecastUploadIntervalSeconds = 600;
    config.safecastDebug = true;
    assert(store.save(config));

    AppConfig loaded;
    assert(store.load(loaded));
    assert(loaded.safecastEnabled);
    assert(std::string(loaded.safecastApiBaseUrl.c_str()) == "https://api.safecast.org");
    assert(loaded.safecastUseTestApi);
    assert(std::string(loaded.safecastCustomApiBaseUrl.c_str()) == "https://example.invalid");
    assert(std::string(loaded.safecastApiKey.c_str()) == "secret-token");
    assert(std::string(loaded.safecastDeviceId.c_str()) == "210");
    assert(std::string(loaded.safecastLatitude.c_str()) == "52.520008");
    assert(std::string(loaded.safecastLongitude.c_str()) == "13.404954");
    assert(std::string(loaded.safecastHeightCm.c_str()) == "100");
    assert(std::string(loaded.safecastLocationName.c_str()) == "Berlin test station");
    assert(std::string(loaded.safecastUnit.c_str()) == "usv");
    assert(loaded.safecastUploadIntervalSeconds == 600);
    assert(loaded.safecastDebug);
}

void testBackupJsonRoundTripsSafecastFields()
{
    AppConfig config;
    config.safecastEnabled = true;
    config.safecastApiBaseUrl = "https://api.safecast.org";
    config.safecastUseTestApi = false;
    config.safecastCustomApiBaseUrl = "https://example.invalid";
    config.safecastApiKey = "secret-token";
    config.safecastDeviceId = "210";
    config.safecastLatitude = "52.520008";
    config.safecastLongitude = "13.404954";
    config.safecastHeightCm = "100";
    config.safecastLocationName = "Berlin test station";
    config.safecastUnit = "cpm";
    config.safecastUploadIntervalSeconds = 300;
    config.safecastDebug = true;

    JsonDocument doc;
    SafecastBackupJson::appendConfig(doc, config);
    assert(doc["safecastEnabled"].as<bool>());
    assert(std::string(doc["safecastApiBaseUrl"].as<const char *>()) == "https://api.safecast.org");
    assert(std::string(doc["safecastCustomApiBaseUrl"].as<const char *>()) == "https://example.invalid");
    assert(std::string(doc["safecastApiKey"].as<const char *>()) == "secret-token");
    assert(doc["safecastUploadIntervalSeconds"].as<uint32_t>() == 300);

    AppConfig restored;
    JsonDocument input;
    input["safecastEnabled"] = true;
    input["safecastApiBaseUrl"] = "https://api.safecast.org";
    input["safecastUseTestApi"] = true;
    input["safecastCustomApiBaseUrl"] = "https://example.invalid";
    input["safecastApiKey"] = "restored-secret";
    input["safecastDeviceId"] = "99";
    input["safecastLatitude"] = "48.137154";
    input["safecastLongitude"] = "11.576124";
    input["safecastHeightCm"] = "150";
    input["safecastLocationName"] = "Munich";
    input["safecastUnit"] = "usv";
    input["safecastUploadIntervalSeconds"] = 900;
    input["safecastDebug"] = true;
    SafecastBackupJson::applyConfig(input.as<JsonVariantConst>(), restored);

    assert(restored.safecastEnabled);
    assert(restored.safecastUseTestApi);
    assert(std::string(restored.safecastApiKey.c_str()) == "restored-secret");
    assert(std::string(restored.safecastDeviceId.c_str()) == "99");
    assert(std::string(restored.safecastUnit.c_str()) == "usv");
    assert(restored.safecastUploadIntervalSeconds == 900);
    assert(restored.safecastDebug);
}

void testBackupJsonClampsUploadIntervalAndRejectsInvalidUnit()
{
    AppConfig restored;
    restored.safecastUnit = "cpm";

    JsonDocument input;
    input["safecastUnit"] = "uSv/h";
    input["safecastUploadIntervalSeconds"] = 10;
    SafecastBackupJson::applyConfig(input.as<JsonVariantConst>(), restored);

    assert(std::string(restored.safecastUnit.c_str()) == "cpm");
    assert(restored.safecastUploadIntervalSeconds == 60);
}
} // namespace

int main()
{
    testAppConfigStorePersistsSafecastFields();
    testBackupJsonRoundTripsSafecastFields();
    testBackupJsonClampsUploadIntervalAndRejectsInvalidUnit();
    std::cout << "safecast settings persistence tests passed\n";
    return 0;
}
