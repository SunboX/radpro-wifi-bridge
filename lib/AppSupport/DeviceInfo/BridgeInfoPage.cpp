#include "BridgeInfoPage.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

BridgeInfoPage::BridgeInfoPage(const PublisherHealth &openSenseMapHealth,
                               const PublisherHealth &gmcMapHealth,
                               const PublisherHealth &radmonHealth,
                               const PublisherHealth &openRadiationHealth)
    : openSenseMapHealth_(openSenseMapHealth),
      gmcMapHealth_(gmcMapHealth),
      radmonHealth_(radmonHealth),
      openRadiationHealth_(openRadiationHealth)
{
}

void BridgeInfoPage::handlePage(WiFiManager *manager)
{
    if (!manager || !manager->server)
        return;

    File file = LittleFS.open("/portal/bridge-info.html", "r");
    if (!file)
    {
        manager->server->send(500, "text/plain", "Bridge info page missing.");
        return;
    }

    String html = file.readString();
    file.close();
    manager->server->send(200, "text/html", html);
}

void BridgeInfoPage::handleJson(WiFiManager *manager)
{
    if (!manager || !manager->server)
        return;
    manager->server->send(200, "application/json", collectJson());
}

String BridgeInfoPage::collectJson() const
{
    JsonDocument doc;
    doc["chipRevision"] = ESP.getChipRevision();
    doc["sdkVersion"] = ESP.getSdkVersion();
    doc["bridgeFirmware"] = BRIDGE_FIRMWARE_VERSION;
    doc["heapFree"] = ESP.getFreeHeap();
    doc["heapMax"] = ESP.getMaxAllocHeap();

    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    String modeStr;
    switch (mode)
    {
    case WIFI_MODE_STA:
        modeStr = "Station";
        break;
    case WIFI_MODE_AP:
        modeStr = "AP";
        break;
    case WIFI_MODE_APSTA:
        modeStr = "AP + Station";
        break;
    default:
        modeStr = "Unknown";
        break;
    }
    doc["wifiMode"] = modeStr;

    IPAddress ip = WiFi.localIP();
    if (ip != IPAddress(0, 0, 0, 0))
        doc["ipAddress"] = ip.toString();
    else
        doc["ipAddress"] = nullptr;

    bool wifiConnected = WiFi.status() == WL_CONNECTED;
    if (wifiConnected)
        doc["wifiRSSI"] = WiFi.RSSI();
    else
        doc["wifiRSSI"] = nullptr;

    doc["macAddress"] = WiFi.macAddress();

    JsonObject publishers = doc["publishers"].to<JsonObject>();
    auto appendHealth = [&publishers](const char *key, const PublisherHealthSnapshot &snapshot) {
        JsonObject json = publishers[key].to<JsonObject>();
        json["enabled"] = snapshot.enabled;
        json["paused"] = snapshot.paused;
        json["pending"] = snapshot.pending;
        json["attempts"] = snapshot.attempts;
        json["successes"] = snapshot.successes;
        json["failures"] = snapshot.failures;
        json["consecutiveFailures"] = snapshot.consecutiveFailures;
        json["lastAttemptMs"] = snapshot.lastAttemptMs;
        json["lastSuccessMs"] = snapshot.lastSuccessMs;
        json["lastFailureMs"] = snapshot.lastFailureMs;
        json["lastStatusCode"] = snapshot.lastStatusCode;
        if (snapshot.lastStatusLine.length())
            json["lastStatusLine"] = snapshot.lastStatusLine;
        else
            json["lastStatusLine"] = nullptr;
        if (snapshot.lastError.length())
            json["lastError"] = snapshot.lastError;
        else
            json["lastError"] = nullptr;
        if (snapshot.lastResponseTrace.length())
            json["lastResponseTrace"] = snapshot.lastResponseTrace;
        else
            json["lastResponseTrace"] = nullptr;
    };

    appendHealth("openSenseMap", openSenseMapHealth_.snapshot());
    appendHealth("gmcMap", gmcMapHealth_.snapshot());
    appendHealth("radmon", radmonHealth_.snapshot());
    appendHealth("openRadiation", openRadiationHealth_.snapshot());

    String json;
    serializeJson(doc, json);
    return json;
}
