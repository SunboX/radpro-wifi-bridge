#include "BridgeInfoPage.h"
#include <ArduinoJson.h>

BridgeInfoPage::BridgeInfoPage() {}

void BridgeInfoPage::handlePage(WiFiManager *manager)
{
    if (!manager || !manager->server)
        return;

    String html;
    html.reserve(6000);
    html += F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'/>"
              "<title>WiFi Bridge Info</title>"
              "<style>body{font-family:Arial,Helvetica,sans-serif;background:#111;color:#eee;margin:0;padding:24px;display:flex;justify-content:center;}"
              ".wrap{max-width:620px;width:100%;}"
              "h1{margin-top:0;}section{background:#1e1e1e;border-radius:8px;padding:16px;margin-bottom:20px;box-shadow:0 4px 18px rgba(0,0,0,0.35);}"
              "dl{display:grid;grid-template-columns:minmax(140px,1fr) minmax(180px,2fr);grid-row-gap:8px;margin:0;}"
              "dt{font-weight:bold;color:#9ecfff;}dd{margin:0;}"
              "button{padding:10px;width:100%;border:none;border-radius:4px;background:#2196F3;color:#fff;font-size:15px;cursor:pointer;margin-top:10px;}"
              "button:hover{background:#1976D2;}a{color:#03A9F4;text-decoration:none;}"
              "</style></head><body class='invert'><div class='wrap'>");
    html += F("<h1>WiFi Bridge Info</h1><section><h2>ESP32-S3</h2><dl>"
              "<dt>Chip revision</dt><dd id='chipRev'>—</dd>"
              "<dt>SDK version</dt><dd id='sdkVersion'>—</dd>"
              "<dt>Bridge firmware</dt><dd id='bridgeFirmware'>—</dd>"
              "<dt>Heap (free)</dt><dd id='heapFree'>—</dd>"
              "<dt>Heap (max)</dt><dd id='heapMax'>—</dd>"
              "<dt>Wi-Fi mode</dt><dd id='wifiMode'>—</dd>"
              "<dt>IPv4</dt><dd id='ipAddress'>—</dd>"
              "<dt>RSSI</dt><dd id='wifiRSSI'>—</dd>"
              "<dt>MAC address</dt><dd id='macAddress'>—</dd>"
              "</dl></section>"
              "<form action='/' method='get'><button type='submit'>Back to Main Menu</button></form>");
    html += F("</div><script>const setField=(id,val)=>{const el=document.getElementById(id);if(!el)return;if(val===null||val===undefined||val===''){el.textContent='—';return;}el.textContent=val;};"
              "async function refreshBridgeInfo(){try{const resp=await fetch('/bridge.json',{cache:'no-store'});if(!resp.ok)throw new Error('bad status');const data=await resp.json();"
              "setField('chipRev',data.chipRevision);"
              "setField('sdkVersion',data.sdkVersion);"
              "setField('bridgeFirmware',data.bridgeFirmware);"
              "setField('heapFree',data.heapFree?`${(data.heapFree/1024).toFixed(1)} kB`:null);"
              "setField('heapMax',data.heapMax?`${(data.heapMax/1024).toFixed(1)} kB`:null);"
              "setField('wifiMode',data.wifiMode);"
              "setField('ipAddress',data.ipAddress);"
              "setField('wifiRSSI',data.wifiRSSI);"
              "setField('macAddress',data.macAddress);"
              "}catch(err){console.warn('Bridge info refresh failed',err);}}refreshBridgeInfo();setInterval(refreshBridgeInfo,4000);</script></body></html>");
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
