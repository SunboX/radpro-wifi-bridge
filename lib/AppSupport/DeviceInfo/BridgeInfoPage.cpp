#include "BridgeInfoPage.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

BridgeInfoPage::BridgeInfoPage() {}

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

    String json;
    serializeJson(doc, json);
    return json;
}
