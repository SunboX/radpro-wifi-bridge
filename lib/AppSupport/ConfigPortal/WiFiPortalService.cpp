#include "ConfigPortal/WiFiPortalService.h"

#include <algorithm>
#include <cstring>

WiFiPortalService::WiFiPortalService(AppConfig &config, AppConfigStore &store, Print &logPort)
    : config_(config),
      store_(store),
      manager_(),
      log_(logPort),
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
      loggingEnabled_(false)
{
}

void WiFiPortalService::begin()
{
    manager_.setDebugOutput(false);
    manager_.setClass("invert");
    manager_.setConnectTimeout(10);
    manager_.setConnectRetries(1);
    manager_.setSaveConfigCallback([this]()
                                   { store_.requestSave(); });
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
        return false;

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
        applyFromParameters(true, true);
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
    manager_.addParameter(&paramMqttHost_);
    manager_.addParameter(&paramMqttPort_);
    manager_.addParameter(&paramMqttClient_);
    manager_.addParameter(&paramMqttUser_);
    manager_.addParameter(&paramMqttPass_);
    manager_.addParameter(&paramMqttTopic_);
    manager_.addParameter(&paramMqttFullTopic_);
    manager_.addParameter(&paramReadInterval_);

    paramsAttached_ = true;
}

bool WiFiPortalService::applyFromParameters(bool persist, bool forceSave)
{
    bool changed = false;
    changed |= UpdateStringIfChanged(config_.deviceName, paramDeviceName_.getValue());
    if (!config_.deviceName.length())
    {
        config_.deviceName = "RadPro WiFi Bridge";
        changed = true;
    }

    changed |= UpdateStringIfChanged(config_.mqttHost, paramMqttHost_.getValue());
    changed |= UpdateStringIfChanged(config_.mqttClient, paramMqttClient_.getValue());
    changed |= UpdateStringIfChanged(config_.mqttUser, paramMqttUser_.getValue());
    changed |= UpdateStringIfChanged(config_.mqttPassword, paramMqttPass_.getValue());
    changed |= UpdateStringIfChanged(config_.mqttTopic, paramMqttTopic_.getValue());
    changed |= UpdateStringIfChanged(config_.mqttFullTopic, paramMqttFullTopic_.getValue());

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
            log_.println("Configuration saved.");
        }
        else
        {
            log_.println("Preferences write failed; configuration not saved.");
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
        break;
    }
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    {
        char ssid[33] = {0};
        size_t len = std::min<size_t>(info.wifi_sta_connected.ssid_len, sizeof(ssid) - 1);
        memcpy(ssid, info.wifi_sta_connected.ssid, len);
        if (lastStatus_ != WL_CONNECTED)
        {
            log_.print("Wi-Fi connected to AP: ");
            log_.println(ssid);
        }
        lastStatus_ = WL_CONNECTED;
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
