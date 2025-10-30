#include "ConfigPortal/WiFiPortalService.h"

#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <esp_wifi_types.h>

WiFiPortalService::WiFiPortalService(AppConfig &config, AppConfigStore &store, Print &logPort, LedController &led)
    : config_(config),
      store_(store),
      manager_(),
      log_(logPort),
      led_(led),
      paramDeviceName_("deviceName", "Device Name", "", kDeviceNameParamLen),
      paramMqttHost_("mqttHost", "MQTT Host", "", kMqttHostParamLen),
      paramMqttPort_("mqttPort", "MQTT Port", "", kMqttPortParamLen),
      paramMqttClient_("mqttClient", "MQTT Client", "", kMqttClientParamLen),
      paramMqttUser_("mqttUser", "MQTT User", "", kMqttUserParamLen),
      paramMqttPass_("mqttPass", "MQTT Password", "", kMqttPassParamLen, "type=\"password\""),
      paramMqttTopic_("mqttTopic", "MQTT Topic", "", kMqttTopicParamLen),
      paramMqttFullTopic_("mqttFullTopic", "MQTT Full Topic", "", kMqttFullTopicParamLen),
      paramReadInterval_("readInterval", "Rad Pro Read Interval (ms)", "", kReadIntervalParamLen),
      paramsAttached_(false),
      lastStatus_(WL_NO_SHIELD),
      wifiEventId_(0),
      lastIp_(),
      hasLoggedIp_(false),
      loggingEnabled_(false),
      pendingReconnect_(false),
      lastReconnectAttemptMs_(0),
      waitingForIpSinceMs_(0)
{
}

void WiFiPortalService::begin()
{
    manager_.setDebugOutput(false);
    manager_.setClass("invert");
    manager_.setConnectTimeout(10);
    manager_.setConnectRetries(1);
    manager_.setTitle("RadPro WiFi Bridge Configuration");
    std::vector<const char *> menuEntries = {"wifi", "custom"};
    manager_.setMenu(menuEntries);
    manager_.setSaveConfigCallback([this]()
                                   { store_.requestSave(); });
    manager_.setSaveParamsCallback([this]() {
        applyFromParameters(false, true);
        store_.requestSave();
    });
    wifiEventId_ = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info)
                                { handleWiFiEvent(event, info); });
    attachParameters();
    refreshParameters();
    logStatusIfNeeded();
}

bool WiFiPortalService::connect(bool forcePortal)
{
    refreshParameters();

    bool connected = false;
    if (forcePortal)
    {
        String apName = config_.deviceName.length() ? config_.deviceName : String("RadPro WiFi Bridge");
        apName += " Setup";
        log_.println("Starting Wi-Fi configuration portal…");
        manager_.setConfigPortalTimeout(0);
        connected = manager_.startConfigPortal(apName.c_str());
    }
    else
    {
        manager_.setConfigPortalTimeout(30);
        connected = manager_.autoConnect(config_.deviceName.c_str());
    }

    if (!connected)
    {
        led_.activateFault(FaultCode::WifiPortalStuck);
        return false;
    }

    applyFromParameters(true);
    logStatusIfNeeded();
    if (WiFi.status() == WL_CONNECTED)
    {
        IPAddress ip = WiFi.localIP();
        if (ip != IPAddress(0, 0, 0, 0))
        {
            IPAddress gw = WiFi.gatewayIP();
            IPAddress mask = WiFi.subnetMask();
            logConnectionDetails(ip, gw, mask);
            lastIp_ = ip;
            hasLoggedIp_ = true;
            led_.clearFault(FaultCode::WifiPortalStuck);
            led_.clearFault(FaultCode::WifiDhcpFailure);
            led_.clearFault(FaultCode::PortalReconnectFailed);
            led_.clearFault(FaultCode::WifiAuthFailure);
        }
    }
    return true;
}

void WiFiPortalService::maintain()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        if (!manager_.getConfigPortalActive() && !manager_.getWebPortalActive())
        {
            refreshParameters();
            manager_.startWebPortal();
        }
    }
    else if (manager_.getWebPortalActive())
    {
        manager_.stopWebPortal();
    }

    logStatusIfNeeded();

    if (waitingForIpSinceMs_ > 0 && WiFi.status() == WL_CONNECTED)
    {
        if (WiFi.localIP() == IPAddress(0, 0, 0, 0))
        {
            unsigned long now = millis();
            if (now - waitingForIpSinceMs_ > 7000)
            {
                led_.activateFault(FaultCode::WifiDhcpFailure);
            }
        }
    }

    if (pendingReconnect_)
    {
        wl_status_t status = WiFi.status();
        if (status == WL_CONNECTED)
        {
            pendingReconnect_ = false;
            hasLoggedIp_ = false;
            logStatusIfNeeded();
            log_.println("Wi-Fi reconnect complete.");
            led_.clearFault(FaultCode::PortalReconnectFailed);
        }
        else
        {
            unsigned long now = millis();
            if (lastReconnectAttemptMs_ == 0 || now - lastReconnectAttemptMs_ >= 5000)
            {
                attemptReconnect();
                if (WiFi.status() != WL_CONNECTED)
                    led_.activateFault(FaultCode::PortalReconnectFailed);
            }
        }
    }
}

void WiFiPortalService::process()
{
    if (manager_.getWebPortalActive() || manager_.getConfigPortalActive())
    {
        manager_.process();
    }
}

void WiFiPortalService::syncIfRequested()
{
    if (store_.consumeSaveRequest())
    {
        bool changed = applyFromParameters(true, true);
        log_.println(changed ? "Configuration updated." : "Configuration saved (no changes).");

        if (manager_.getConfigPortalActive())
        {
            manager_.stopConfigPortal();
        }

        pendingReconnect_ = true;
        lastReconnectAttemptMs_ = 0;
        hasLoggedIp_ = false;
        log_.println("Wi-Fi reconnect scheduled.");
        led_.activateFault(FaultCode::PortalReconnectFailed);
    }
}

void WiFiPortalService::dumpStatus()
{
    logStatus();
}

void WiFiPortalService::enableStatusLogging()
{
    if (loggingEnabled_)
        return;

    loggingEnabled_ = true;
    hasLoggedIp_ = false;
    logStatus();
}

void WiFiPortalService::refreshParameters()
{
    manager_.setHostname(config_.deviceName.c_str());

    paramDeviceName_.setValue(config_.deviceName.c_str(), kDeviceNameParamLen);
    paramMqttHost_.setValue(config_.mqttHost.c_str(), kMqttHostParamLen);

    String portStr = String(config_.mqttPort);
    paramMqttPort_.setValue(portStr.c_str(), kMqttPortParamLen);

    paramMqttClient_.setValue(config_.mqttClient.c_str(), kMqttClientParamLen);
    paramMqttUser_.setValue(config_.mqttUser.c_str(), kMqttUserParamLen);
    paramMqttPass_.setValue(config_.mqttPassword.c_str(), kMqttPassParamLen);
    paramMqttTopic_.setValue(config_.mqttTopic.c_str(), kMqttTopicParamLen);
    paramMqttFullTopic_.setValue(config_.mqttFullTopic.c_str(), kMqttFullTopicParamLen);

    String intervalStr = String(config_.readIntervalMs);
    paramReadInterval_.setValue(intervalStr.c_str(), kReadIntervalParamLen);

    attachParameters();
}

void WiFiPortalService::attachParameters()
{
    if (paramsAttached_)
        return;

    manager_.addParameter(&paramDeviceName_);

    manager_.setCustomMenuHTML("<div style='margin:-5px;display:flex;flex-direction:column;gap:20px;'>"
                               "<form action='/mqtt' method='get'><button class='btn btn-primary' type='submit'>Configure MQTT</button></form>"
                               "<form action='/restart' method='get'><button class='btn btn-primary' type='submit'>Restart Device</button></form>"
                               "</div>");

    manager_.setWebServerCallback([this]() {
        if (!manager_.server)
            return;

        manager_.server->on("/mqtt", HTTP_GET, [this]() {
            sendMqttForm();
        });

        manager_.server->on("/mqtt", HTTP_POST, [this]() {
            handleMqttPost();
        });

        manager_.server->on("/restart", HTTP_GET, [this]() {
            manager_.server->send(200, "text/plain", "Restarting...\n");
            log_.println("Restart requested from Wi-Fi portal.");
            delay(200);
            ESP.restart();
        });
    });

    paramsAttached_ = true;
}

bool WiFiPortalService::applyFromParameters(bool persist, bool forceSave)
{
    auto readTrimmed = [](const char *value) -> String {
        String result = value ? String(value) : String();
        result.trim();
        return result;
    };

    bool changed = false;

    String newDeviceName = readTrimmed(paramDeviceName_.getValue());
    if (!newDeviceName.length())
        newDeviceName = "RadPro WiFi Bridge";
    if (config_.deviceName != newDeviceName)
        changed = true;
    config_.deviceName = newDeviceName;

    String newMqttHost = readTrimmed(paramMqttHost_.getValue());
    if (config_.mqttHost != newMqttHost)
        changed = true;
    config_.mqttHost = newMqttHost;

    String newMqttClient = readTrimmed(paramMqttClient_.getValue());
    if (config_.mqttClient != newMqttClient)
        changed = true;
    config_.mqttClient = newMqttClient;

    String newMqttUser = readTrimmed(paramMqttUser_.getValue());
    if (config_.mqttUser != newMqttUser)
        changed = true;
    config_.mqttUser = newMqttUser;

    String newMqttPassword = readTrimmed(paramMqttPass_.getValue());
    if (config_.mqttPassword != newMqttPassword)
        changed = true;
    config_.mqttPassword = newMqttPassword;

    String newMqttTopic = readTrimmed(paramMqttTopic_.getValue());
    if (config_.mqttTopic != newMqttTopic)
        changed = true;
    config_.mqttTopic = newMqttTopic;

    String newMqttFullTopic = readTrimmed(paramMqttFullTopic_.getValue());
    if (config_.mqttFullTopic != newMqttFullTopic)
        changed = true;
    config_.mqttFullTopic = newMqttFullTopic;

    uint32_t newInterval = strtoul(paramReadInterval_.getValue(), nullptr, 10);
    if (newInterval < kMinReadIntervalMs)
        newInterval = kMinReadIntervalMs;
    if (newInterval != config_.readIntervalMs)
    {
        config_.readIntervalMs = newInterval;
        changed = true;
    }

    uint32_t parsedPort = strtoul(paramMqttPort_.getValue(), nullptr, 10);
    if (parsedPort == 0 || parsedPort > 65535)
        parsedPort = config_.mqttPort;
    if (config_.mqttPort != parsedPort)
    {
        config_.mqttPort = static_cast<uint16_t>(parsedPort);
        changed = true;
    }

    WiFi.setHostname(config_.deviceName.c_str());
    refreshParameters();

    if (persist && (changed || forceSave))
    {
        if (store_.save(config_))
        {
            log_.print("Configuration saved to NVS (mqttHost='");
            log_.print(config_.mqttHost);
            log_.println("').");
            led_.clearFault(FaultCode::NvsWriteFailure);
        }
        else
        {
            log_.println("Preferences write failed; configuration not saved.");
            led_.activateFault(FaultCode::NvsWriteFailure);
        }
    }

    return changed;
}

void WiFiPortalService::handleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    {
        IPAddress ip(info.got_ip.ip_info.ip.addr);
        IPAddress gateway(info.got_ip.ip_info.gw.addr);
        IPAddress netmask(info.got_ip.ip_info.netmask.addr);
        logConnectionDetails(ip, gateway, netmask);
        lastStatus_ = WL_CONNECTED;
        waitingForIpSinceMs_ = 0;
        led_.clearFault(FaultCode::WifiDhcpFailure);
        led_.clearFault(FaultCode::WifiPortalStuck);
        led_.clearFault(FaultCode::PortalReconnectFailed);
        led_.clearFault(FaultCode::WifiAuthFailure);
        break;
    }
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    {
        char ssid[33] = {0};
        size_t len = std::min<size_t>(info.wifi_sta_connected.ssid_len, sizeof(ssid) - 1);
        memcpy(ssid, info.wifi_sta_connected.ssid, len);
        lastKnownSsid_ = String(ssid);
        String passCandidate = WiFi.psk();
        if (passCandidate.length())
            lastKnownPass_ = passCandidate;
        if (lastStatus_ != WL_CONNECTED)
        {
            log_.print("Wi-Fi connected to AP: ");
            log_.println(ssid);
        }
        lastStatus_ = WL_CONNECTED;
        waitingForIpSinceMs_ = millis();
        led_.clearFault(FaultCode::WifiAuthFailure);
        break;
    }
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        if (lastStatus_ != WL_DISCONNECTED)
        {
            log_.print("Wi-Fi disconnected (reason ");
            log_.print(info.wifi_sta_disconnected.reason);
            log_.println(")");
        }
        lastStatus_ = WL_DISCONNECTED;
        hasLoggedIp_ = false;
        lastIp_ = IPAddress();
        pendingReconnect_ = true;
        lastReconnectAttemptMs_ = 0;
        waitingForIpSinceMs_ = 0;
        switch (info.wifi_sta_disconnected.reason)
        {
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_ASSOC_EXPIRE:
        case WIFI_REASON_ASSOC_LEAVE:
        case WIFI_REASON_CONNECTION_FAIL:
            led_.activateFault(FaultCode::WifiAuthFailure);
            break;
        default:
            led_.activateFault(FaultCode::WifiDhcpFailure);
            break;
        }
        break;
    default:
        break;
    }
}

void WiFiPortalService::logStatusIfNeeded()
{
    wl_status_t current = WiFi.status();
    if (current != lastStatus_)
    {
        lastStatus_ = current;
        if (current == WL_CONNECTED)
        {
            hasLoggedIp_ = false;
            if (loggingEnabled_)
                logStatus();
        }
        else
        {
            lastIp_ = IPAddress();
            hasLoggedIp_ = false;
            if (loggingEnabled_)
                log_.println("Wi-Fi not connected.");
        }
    }
    else if (current == WL_CONNECTED && !hasLoggedIp_)
    {
        IPAddress ip = WiFi.localIP();
        if (ip != IPAddress(0, 0, 0, 0))
        {
            logConnectionDetails(ip, WiFi.gatewayIP(), WiFi.subnetMask());
        }
    }
}

void WiFiPortalService::logConnectionDetails(const IPAddress &ip, const IPAddress &gateway, const IPAddress &mask)
{
    if (ip == IPAddress(0, 0, 0, 0))
        return;
    bool alreadyLogged = hasLoggedIp_ && (ip == lastIp_);
    lastIp_ = ip;

    if (!loggingEnabled_)
    {
        hasLoggedIp_ = false;
        return;
    }

    if (alreadyLogged)
        return;

    log_.print("Wi-Fi connected: ");
    log_.print(WiFi.SSID());
    log_.print(" (");
    log_.print(ip);
    log_.println(")");
    log_.print("Gateway: ");
    log_.print(gateway);
    log_.print("  Mask: ");
    log_.println(mask);
    log_.print("RSSI: ");
    log_.print(WiFi.RSSI());
    log_.println(" dBm");

    hasLoggedIp_ = true;
}

void WiFiPortalService::logStatus()
{
    if (!loggingEnabled_)
        return;

    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED)
    {
        logConnectionDetails(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask());
    }
    else
    {
        log_.println("Wi-Fi not connected.");
    }
}

void WiFiPortalService::attemptReconnect()
{
    lastReconnectAttemptMs_ = millis();
    log_.println("Wi-Fi reconnect pending; attempting to rejoin.");

    WiFi.mode(WIFI_STA);

    if (WiFi.reconnect())
    {
        log_.println("Wi-Fi reconnect requested via reconnect().");
        return;
    }

    String ssid = lastKnownSsid_.length() ? lastKnownSsid_ : WiFi.SSID();
    String pass = lastKnownPass_.length() ? lastKnownPass_ : WiFi.psk();

    if (ssid.length())
    {
        if (pass.length())
        {
            log_.print("Wi-Fi.begin(");
            log_.print(ssid);
            log_.println(") with passphrase.");
            WiFi.begin(ssid.c_str(), pass.c_str());
        }
        else
        {
            log_.print("Wi-Fi.begin(");
            log_.print(ssid);
            log_.println(") without passphrase.");
            WiFi.begin(ssid.c_str());
        }
    }
    else
    {
        log_.println("Wi-Fi.begin() with stored credentials.");
        WiFi.begin();
    }
}

void WiFiPortalService::sendMqttForm(const String &message)
{
    if (!manager_.server)
        return;

    String html;
    html.reserve(2048);
    html += F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'/><title>Configure MQTT</title><style>body{font-family:Arial,Helvetica,sans-serif;background:#111;color:#eee;margin:0;padding:24px;display:flex;justify-content:center;}h1{margin-top:0;}form{display:flex;flex-direction:column;gap:12px;width:100%;}label{font-weight:bold;}input,select{padding:8px;border-radius:4px;border:1px solid #666;background:#222;color:#eee;width:100%;}button{padding:10px;border:none;border-radius:4px;background:#2196F3;color:#fff;font-size:15px;cursor:pointer;width:100%;}button:hover{background:#1976D2;}a{color:#03A9F4;} .wrap{display:inline-block;min-width:260px;max-width:500px;width:100%;text-align:left;} p.notice{margin:0 0 12px 0;color:#8bc34a;}</style></head><body class='invert'><div class='wrap'><h1>MQTT Settings</h1>");

    if (message.length())
    {
        html += F("<p class='notice'>");
        html += message;
        html += F("</p>");
    }

    html += F("<form method='POST' action='/mqtt'>");

    html += F("<label for='mqttHost'>Host</label><input id='mqttHost' name='mqttHost' type='text' style='box-sizing:border-box;' value='");
    html += htmlEscape(config_.mqttHost);
    html += F("'/>");

    html += F("<label for='mqttPort'>Port</label><input id='mqttPort' name='mqttPort' type='number' min='1' max='65535' style='box-sizing:border-box;' value='");
    html += String(config_.mqttPort);
    html += F("'/>");

    html += F("<label for='mqttClient'>Client ID Suffix</label><input id='mqttClient' name='mqttClient' type='text' style='box-sizing:border-box;' value='");
    html += htmlEscape(config_.mqttClient);
    html += F("'/>");

    html += F("<label for='mqttUser'>Username</label><input id='mqttUser' name='mqttUser' type='text' style='box-sizing:border-box;' value='");
    html += htmlEscape(config_.mqttUser);
    html += F("'/>");

    html += F("<label for='mqttPass'>Password</label><input id='mqttPass' name='mqttPass' type='password' style='box-sizing:border-box;' value='");
    html += htmlEscape(config_.mqttPassword);
    html += F("'/>");

    html += F("<label for='mqttTopic'>Base Topic</label><input id='mqttTopic' name='mqttTopic' type='text' style='box-sizing:border-box;' value='");
    html += htmlEscape(config_.mqttTopic);
    html += F("'/>");

    html += F("<label for='mqttFullTopic'>Full Topic Template</label><input id='mqttFullTopic' name='mqttFullTopic' type='text' style='box-sizing:border-box;' value='");
    html += htmlEscape(config_.mqttFullTopic);
    html += F("'/>");

    html += F("<label for='readInterval'>Read Interval (ms)</label><input id='readInterval' name='readInterval' type='number' style='box-sizing:border-box;' min='");
    html += String(kMinReadIntervalMs);
    html += F("' value='");
    html += String(config_.readIntervalMs);
    html += F("'/>");

    html += F("<button type='submit'>Save MQTT Settings</button></form>"
              "<form action='/' method='get' style='margin-top:20px;'><button class='btn btn-primary' type='submit'>Main menu</button></form>"
              "</div></body></html>");

    manager_.server->send(200, "text/html", html);
}

void WiFiPortalService::handleMqttPost()
{
    if (!manager_.server)
        return;

    auto &server = *manager_.server;
    String host = server.arg("mqttHost");
    String portStr = server.arg("mqttPort");
    String client = server.arg("mqttClient");
    String user = server.arg("mqttUser");
    String pass = server.arg("mqttPass");
    String topic = server.arg("mqttTopic");
    String fullTopic = server.arg("mqttFullTopic");
    String intervalStr = server.arg("readInterval");

    host.trim();
    client.trim();
    user.trim();
    pass.trim();
    topic.trim();
    fullTopic.trim();
    intervalStr.trim();

    bool changed = false;
    changed |= UpdateStringIfChanged(config_.mqttHost, host.c_str());
    changed |= UpdateStringIfChanged(config_.mqttClient, client.c_str());
    changed |= UpdateStringIfChanged(config_.mqttUser, user.c_str());
    changed |= UpdateStringIfChanged(config_.mqttPassword, pass.c_str());
    changed |= UpdateStringIfChanged(config_.mqttTopic, topic.c_str());
    changed |= UpdateStringIfChanged(config_.mqttFullTopic, fullTopic.c_str());

    uint32_t parsedPort = strtoul(portStr.c_str(), nullptr, 10);
    if (parsedPort == 0 || parsedPort > 65535)
        parsedPort = config_.mqttPort;
    if (config_.mqttPort != parsedPort)
    {
        config_.mqttPort = static_cast<uint16_t>(parsedPort);
        changed = true;
    }

    uint32_t newInterval = strtoul(intervalStr.c_str(), nullptr, 10);
    if (newInterval < kMinReadIntervalMs)
        newInterval = kMinReadIntervalMs;
    if (config_.readIntervalMs != newInterval)
    {
        config_.readIntervalMs = newInterval;
        changed = true;
    }

    String message;
    if (changed)
    {
        if (store_.save(config_))
        {
            log_.println("MQTT configuration updated via portal.");
            led_.clearFault(FaultCode::NvsWriteFailure);
            pendingReconnect_ = true;
            lastReconnectAttemptMs_ = 0;
            hasLoggedIp_ = false;
            message = F("Settings saved. The device will reconnect using the new MQTT configuration.");
        }
        else
        {
            led_.activateFault(FaultCode::NvsWriteFailure);
            message = F("Failed to save settings to NVS.");
        }
    }
    else
    {
        message = F("No changes detected.");
    }

    sendMqttForm(message);
}

String WiFiPortalService::htmlEscape(const String &value)
{
    String out;
    out.reserve(value.length());
    for (size_t i = 0; i < value.length(); ++i)
    {
        char c = value[i];
        switch (c)
        {
        case '&':
            out += F("&amp;");
            break;
        case '<':
            out += F("&lt;");
            break;
        case '>':
            out += F("&gt;");
            break;
        case '"':
            out += F("&quot;");
            break;
        case '\'':
            out += F("&#39;");
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}
