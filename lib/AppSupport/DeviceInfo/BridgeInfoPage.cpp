#include "BridgeInfoPage.h"

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

static String escapeJson(const String &value)
{
    String out;
    out.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i)
    {
        char c = value[i];
        switch (c)
        {
        case '\"':
        case '\\':
            out += '\\';
            out += c;
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char buf[7];
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                out += buf;
            }
            else
            {
                out += c;
            }
            break;
        }
    }
    return out;
}

String BridgeInfoPage::collectJson() const
{
    auto appendStringField = [](String &json, const char *key, const String &value) {
        json += '\"';
        json += key;
        json += '\"';
        json += ':';
        if (value.length())
        {
            json += '\"';
            json += escapeJson(value);
            json += '\"';
        }
        else
        {
            json += "null";
        }
        json += ',';
    };

    auto appendNumberField = [](String &json, const char *key, long value, bool valid = true) {
        json += '\"';
        json += key;
        json += '\"';
        json += ':';
        if (valid)
            json += String(value);
        else
            json += "null";
        json += ',';
    };

    String json;
    json.reserve(512);
    json += '{';
    appendNumberField(json, "chipRevision", ESP.getChipRevision());
    appendStringField(json, "sdkVersion", String(ESP.getSdkVersion()));
    appendStringField(json, "bridgeFirmware", String(BRIDGE_FIRMWARE_VERSION));
    appendNumberField(json, "heapFree", ESP.getFreeHeap());
    appendNumberField(json, "heapMax", ESP.getMaxAllocHeap());

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
    appendStringField(json, "wifiMode", modeStr);

    IPAddress ip = WiFi.localIP();
    appendStringField(json, "ipAddress", (ip != IPAddress(0, 0, 0, 0)) ? ip.toString() : String());

    bool wifiConnected = WiFi.status() == WL_CONNECTED;
    appendNumberField(json, "wifiRSSI", wifiConnected ? WiFi.RSSI() : 0, wifiConnected);

    appendStringField(json, "macAddress", WiFi.macAddress());

    if (!json.isEmpty() && json[json.length() - 1] == ',')
        json.setCharAt(json.length() - 1, '}');
    else
        json += '}';
    return json;
}
