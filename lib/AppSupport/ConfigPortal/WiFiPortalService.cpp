#include "ConfigPortal/WiFiPortalService.h"

#include "FileSystem/BridgeFileSystem.h"
#include "Ota/OtaUpdateService.h"
#include "Mqtt/MqttPublisher.h"
#include "OpenSenseMap/OpenSenseMapPublisher.h"
#include "GmcMap/GmcMapPublisher.h"
#include "Radmon/RadmonPublisher.h"

#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <esp_wifi.h>
#include <ArduinoJson.h>

WiFiPortalService::WiFiPortalService(AppConfig &config, AppConfigStore &store, DeviceInfoStore &info, DebugLogStream &logPort, LedController &led)
    : config_(config),
      store_(store),
      deviceInfo_(info),
      deviceInfoPage_(info),
      bridgeInfoPage_(),
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
      paramGmcAccount_("gmcAccount", "GMCMap Account ID", "", 16),
      paramGmcDevice_("gmcDevice", "GMCMap Device ID", "", 24),
      paramRadmonUser_("radmonUser", "Radmon Username", "", kRadmonUserLen),
      paramRadmonPassword_("radmonPass", "Radmon Password", "", kRadmonPasswordLen, "type=\"password\""),
      paramsAttached_(false),
      lastStatus_(WL_NO_SHIELD),
      wifiEventId_(0),
      lastIp_(),
      hasLoggedIp_(false),
      loggingEnabled_(false),
      pendingReconnect_(false),
      lastReconnectAttemptMs_(0),
      waitingForIpSinceMs_(0),
      portalPsDisabled_(false),
      previousPsType_(WIFI_PS_MIN_MODEM)
{
}

void WiFiPortalService::begin()
{
    manager_.setDebugOutput(true);
    manager_.setClass("invert");
    manager_.setConnectTimeout(10);
    manager_.setConnectRetries(1);
    manager_.setTitle("RadPro WiFi Bridge Configuration");
    manager_.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    manager_.setWiFiAPChannel(1);
    std::vector<const char *> menuEntries = {"wifi", "custom"};
    manager_.setMenu(menuEntries);
    log_.print(F("WiFi portal menu tokens: "));
    for (size_t i = 0; i < menuEntries.size(); ++i)
    {
        log_.print(menuEntries[i]);
        if (i + 1 < menuEntries.size())
            log_.print(F(", "));
    }
    log_.println();
    manager_.setSaveConfigCallback([this]()
                                   { store_.requestSave(); });
    manager_.setSaveParamsCallback([this]()
                                   {
        applyFromParameters(false, true);
        store_.requestSave(); });
    wifiEventId_ = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info)
                                { handleWiFiEvent(event, info); });
    attachParameters();
    refreshParameters();
    logStatusIfNeeded();
    dumpFilesystemContents(F("WiFiPortalService begin"));
    logPortalState("begin");
}

void WiFiPortalService::setOtaStartCallback(std::function<void()> cb)
{
    onOtaStart_ = std::move(cb);
}

void WiFiPortalService::notifyOtaStart()
{
    if (otaHooksFired_)
        return;
    otaHooksFired_ = true;
    if (onOtaStart_)
        onOtaStart_();
}

bool WiFiPortalService::connect(bool forcePortal)
{
    log_.print(F("WiFiPortalService::connect(forcePortal="));
    log_.print(forcePortal ? F("true") : F("false"));
    log_.println(F(") invoked."));
    logPortalState("connect(start)");

    refreshParameters();

    bool haveStoredCredentials = hasStoredCredentials();
    bool connected = false;
    if (!forcePortal && !haveStoredCredentials)
    {
        forcePortal = true;
        log_.println(F("No saved Wi-Fi credentials detected; forcing configuration portal."));
    }

    onboardingMode_ = forcePortal && !haveStoredCredentials;

    if (forcePortal)
    {
        String apName = config_.deviceName.length() ? config_.deviceName : String("RadPro WiFi Bridge");
        apName += " Setup";
        log_.print(F("Starting Wi-Fi configuration portal with SSID '"));
        log_.print(apName);
        log_.println(F("'"));
        if (onboardingMode_)
        {
            log_.println(F("Onboarding: switching to AP-only mode to keep the captive portal stable."));
            prepareConfigPortalAp(apName);
        }
        manager_.setConfigPortalTimeout(0);
        connected = manager_.startConfigPortal(apName.c_str());
        log_.println(connected ? F("Configuration portal completed (credentials supplied).") : F("Configuration portal exited without connection."));
        logPortalState("after startConfigPortal");
    }
    else
    {
        manager_.setConfigPortalTimeout(30);
        log_.println(F("Attempting Wi-Fi autoConnect()…"));
        connected = manager_.autoConnect(config_.deviceName.c_str());
        log_.println(connected ? F("autoConnect() succeeded.") : F("autoConnect() failed or timed out."));
        if (!connected)
            logPortalState("autoConnect failed");
    }

    if (!connected)
    {
        led_.activateFault(FaultCode::WifiPortalStuck);
        log_.println(F("WiFiPortalService::connect() returning failure."));
        return false;
    }

    applyFromParameters(true);
    logStatusIfNeeded();
    if (WiFi.status() == WL_CONNECTED)
    {
        onboardingMode_ = false;
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
    const bool connected = WiFi.status() == WL_CONNECTED;
    const bool configPortalActive = manager_.getConfigPortalActive();
    const bool webPortalActive = manager_.getWebPortalActive();
    bool haveStoredCredentials = hasStoredCredentials();
    if (onboardingMode_ && haveStoredCredentials)
    {
        onboardingMode_ = false;
        if (!configPortalActive && !connected)
        {
            pendingReconnect_ = true;
            lastReconnectAttemptMs_ = 0;
        }
    }
    const bool onboarding = onboardingMode_ || !haveStoredCredentials;

    if (connected)
    {
        if (!configPortalActive && !webPortalActive)
        {
            refreshParameters();
            if (!menuHtml_.length())
            {
                log_.println(F("Menu HTML empty prior to portal start; attempting reload."));
                ensureMenuHtmlLoaded();
            }
            log_.print(F("Starting Wi-Fi web portal; menu bytes="));
            log_.print(menuHtml_.length());
            log_.print(F(" routes registered="));
            log_.println(routesRegistered_ ? F("true") : F("false"));
            manager_.startWebPortal();
            log_.println(manager_.getWebPortalActive() ? F("Wi-Fi web portal started.") : F("Wi-Fi web portal inactive after start request."));
            logPortalState("startWebPortal");
        }
    }
    else
    {
        // Stay in AP/config portal when not connected or when onboarding (no creds).
        if (webPortalActive && (configPortalActive || onboarding))
        {
            // Keep the portal running for setup; do not stop.
            logPortalState("webPortalActive-keep");
        }
        else if (webPortalActive)
        {
            log_.println(F("Stopping Wi-Fi web portal (station disconnected)."));
            manager_.stopWebPortal();
            logPortalState("stopWebPortal");
        }
        else if (!webPortalActive && onboarding && !configPortalActive)
        {
            // Edge case: lost credentials or first boot – ensure portal is available.
            ensureMenuHtmlLoaded();
            log_.println(F("Starting Wi-Fi web portal for onboarding (no credentials detected)."));
            manager_.startWebPortal();
            logPortalState("startWebPortalOnboarding");
        }
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
        if (configPortalActive || onboarding)
        {
            pendingReconnect_ = false;
            lastReconnectAttemptMs_ = 0;
            waitingForIpSinceMs_ = 0;
            log_.println(F("Reconnect suppressed while captive portal is active/onboarding."));
        }
        else
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

    if (restartScheduled_)
    {
        if (millis() >= restartAtMs_)
        {
            log_.println(F("Restarting device to apply configuration changes."));
            delay(100);
            ESP.restart();
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
        logPortalState("syncIfRequested");
    }
}

void WiFiPortalService::dumpStatus()
{
    logStatus();
    logPortalState("dumpStatus");
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

    paramGmcAccount_.setValue(config_.gmcMapAccountId.c_str(), 16);
    paramGmcDevice_.setValue(config_.gmcMapDeviceId.c_str(), 24);
    paramRadmonUser_.setValue(config_.radmonUser.c_str(), kRadmonUserLen);
    paramRadmonPassword_.setValue(config_.radmonPassword.c_str(), kRadmonPasswordLen);

    attachParameters();
}

void WiFiPortalService::attachParameters()
{
    if (paramsAttached_)
    {
        log_.println(F("attachParameters(): parameters already attached; skipping."));
        return;
    }

    log_.println(F("attachParameters(): registering Wi-Fi portal parameters and routes."));

    manager_.addParameter(&paramDeviceName_);

    ensureMenuHtmlLoaded();
    if (menuHtml_.length())
    {
        log_.print(F("Applying custom menu HTML ("));
        log_.print(menuHtml_.length());
        log_.println(F(" bytes)."));
        applyMenuHtmlForLocale(resolvePortalLocale());
    }
    else
    {
        log_.println(F("Custom menu HTML missing; Wi-Fi portal menu will only show default entries."));
    }

    manager_.setWebServerCallback([this]()
                                  {
        log_.println(F("Web server callback invoked; registering custom portal routes."));
        if (!manager_.server)
        {
            log_.println(F("manager_.server is null; cannot register custom routes."));
            return;
        }
        routesRegistered_ = true;
        log_.println(F("Custom Wi-Fi portal routes: /mqtt /osem /radmon /gmc /device /device.json /bridge /bridge.json /backup /backup.json /backup/restore /logs /logs.json /ota /ota/status /ota/fetch /ota/upload/* /restart"));

        manager_.server->on("/mqtt", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /mqtt"));
            MqttPublisher::SendPortalForm(*this);
        });

        manager_.server->on("/portal/portal.css", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /portal/portal.css"));
            if (!sendStaticFile("/portal/portal.css", "text/css"))
                sendTemplateError("/portal/portal.css");
        });

        manager_.server->on("/portal/js/device-info.js", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /portal/js/device-info.js"));
            if (!sendStaticFile("/portal/js/device-info.js", "application/javascript"))
                sendTemplateError("/portal/js/device-info.js");
        });

        manager_.server->on("/portal/js/bridge-info.js", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /portal/js/bridge-info.js"));
            if (!sendStaticFile("/portal/js/bridge-info.js", "application/javascript"))
                sendTemplateError("/portal/js/bridge-info.js");
        });

        manager_.server->on("/portal/js/backup-page.js", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /portal/js/backup-page.js"));
            if (!sendStaticFile("/portal/js/backup-page.js", "application/javascript"))
                sendTemplateError("/portal/js/backup-page.js");
        });

        manager_.server->on("/portal/js/log-console.js", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /portal/js/log-console.js"));
            if (!sendStaticFile("/portal/js/log-console.js", "application/javascript"))
                sendTemplateError("/portal/js/log-console.js");
        });

        manager_.server->on("/portal/portal-locale.js", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /portal/portal-locale.js"));
            if (!sendStaticFile("/portal/portal-locale.js", "application/javascript"))
                sendTemplateError("/portal/portal-locale.js");
        });

        manager_.server->on("/portal/locales/en.json", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /portal/locales/en.json"));
            if (!sendStaticFile("/portal/locales/en.json", "application/json"))
                sendTemplateError("/portal/locales/en.json");
        });

        manager_.server->on("/portal/locales/de.json", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /portal/locales/de.json"));
            if (!sendStaticFile("/portal/locales/de.json", "application/json"))
                sendTemplateError("/portal/locales/de.json");
        });

        manager_.server->on("/portal/js/ota-page.js", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /portal/js/ota-page.js"));
            if (!sendStaticFile("/portal/js/ota-page.js", "application/javascript"))
                sendTemplateError("/portal/js/ota-page.js");
        });

        manager_.server->on("/portal/js/jszip.min.js", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /portal/js/jszip.min.js"));
            if (!sendStaticFile("/portal/js/jszip.min.js", "application/javascript"))
                sendTemplateError("/portal/js/jszip.min.js");
        });

        manager_.server->on("/mqtt", HTTP_POST, [this]() {
            log_.println(F("HTTP POST /mqtt"));
            if (!manager_.server)
                return;
            String message;
            bool needsReconnect = MqttPublisher::HandlePortalPost(*manager_.server, config_, store_, led_, log_, message);
            if (needsReconnect)
            {
                pendingReconnect_ = true;
                lastReconnectAttemptMs_ = 0;
                hasLoggedIp_ = false;
            }
            MqttPublisher::SendPortalForm(*this, message);
        });

        manager_.server->on("/osem", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /osem"));
            OpenSenseMapPublisher::SendPortalForm(*this);
        });

        manager_.server->on("/osem", HTTP_POST, [this]() {
            log_.println(F("HTTP POST /osem"));
            if (!manager_.server)
                return;
            String message;
            OpenSenseMapPublisher::HandlePortalPost(*manager_.server, config_, store_, led_, log_, message);
            OpenSenseMapPublisher::SendPortalForm(*this, message);
        });

        manager_.server->on("/radmon", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /radmon"));
            RadmonPublisher::SendPortalForm(*this);
        });

        manager_.server->on("/radmon", HTTP_POST, [this]() {
            log_.println(F("HTTP POST /radmon"));
            if (!manager_.server)
                return;
            String message;
            RadmonPublisher::HandlePortalPost(*manager_.server, config_, store_, led_, log_, message);
            RadmonPublisher::SendPortalForm(*this, message);
        });

        manager_.server->on("/gmc", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /gmc"));
            GmcMapPublisher::SendPortalForm(*this);
        });

        manager_.server->on("/gmc", HTTP_POST, [this]() {
            log_.println(F("HTTP POST /gmc"));
            if (!manager_.server)
                return;
            String message;
            GmcMapPublisher::HandlePortalPost(*manager_.server, config_, store_, led_, log_, message);
            GmcMapPublisher::SendPortalForm(*this, message);
        });

        manager_.server->on("/device", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /device"));
            TemplateReplacements vars;
            appendCommonTemplateVars(vars);
            sendTemplate("/portal/device-info.html", vars);
        });

        manager_.server->on("/device.json", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /device.json"));
            deviceInfoPage_.handleJson(&manager_);
        });

        manager_.server->on("/bridge", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /bridge"));
            TemplateReplacements vars;
            appendCommonTemplateVars(vars);
            sendTemplate("/portal/bridge-info.html", vars);
        });

        manager_.server->on("/bridge.json", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /bridge.json"));
            bridgeInfoPage_.handleJson(&manager_);
        });

        manager_.server->on("/logs", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /logs"));
            TemplateReplacements vars;
            appendCommonTemplateVars(vars);
            sendTemplate("/portal/logs.html", vars);
        });

        manager_.server->on("/logs.json", HTTP_GET, [this]() {
            handleLogsJson();
        });

        manager_.server->on("/backup", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /backup"));
            sendConfigBackupPage();
        });

        manager_.server->on("/backup.json", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /backup.json"));
            handleConfigDownload();
        });

        manager_.server->on("/backup/restore", HTTP_POST, [this]() {
            log_.println(F("HTTP POST /backup/restore"));
            handleConfigRestore();
        });

        manager_.server->on("/ota", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /ota"));
            sendOtaPage();
        });

        manager_.server->on("/ota/status", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /ota/status"));
            handleOtaStatus();
        });

        manager_.server->on("/ota/fetch", HTTP_POST, [this]() {
            log_.println(F("HTTP POST /ota/fetch"));
            handleOtaFetch();
        });

        manager_.server->on("/ota/upload/begin", HTTP_POST, [this]() {
            log_.println(F("HTTP POST /ota/upload/begin"));
            handleOtaUploadBegin();
        });

        manager_.server->on("/ota/upload/part/begin", HTTP_POST, [this]() {
            log_.println(F("HTTP POST /ota/upload/part/begin"));
            handleOtaUploadPartBegin();
        });

        manager_.server->on("/ota/upload/part/chunk", HTTP_POST, [this]() {
            handleOtaUploadPartChunk();
        });

        manager_.server->on("/ota/upload/part/finish", HTTP_POST, [this]() {
            log_.println(F("HTTP POST /ota/upload/part/finish"));
            handleOtaUploadPartFinish();
        });

        manager_.server->on("/ota/upload/finish", HTTP_POST, [this]() {
            log_.println(F("HTTP POST /ota/upload/finish"));
            handleOtaUploadFinish();
        });

        manager_.server->on("/ota/cancel", HTTP_POST, [this]() {
            log_.println(F("HTTP POST /ota/cancel"));
            handleOtaCancel();
        });

        manager_.server->on("/restart", HTTP_GET, [this]() {
            log_.println(F("HTTP GET /restart"));
            manager_.server->send(200, "text/plain", "Restarting...\n");
            log_.println("Restart requested from Wi-Fi portal.");
            delay(200);
            ESP.restart();
        });

        manager_.server->onNotFound([this]() {
            if (!manager_.server)
                return;
            log_.print(F("Portal request not found: "));
            log_.print(manager_.server->uri());
            log_.print(F(" (method "));
            log_.print(manager_.server->method());
            log_.println(F(")"));
            manager_.handleNotFound();
        }); });

    log_.println(F("Custom Wi-Fi portal routes registered."));
    paramsAttached_ = true;
}

bool WiFiPortalService::applyFromParameters(bool persist, bool forceSave)
{
    auto readTrimmed = [](const char *value) -> String
    {
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
    case ARDUINO_EVENT_WIFI_AP_START:
        log_.print(F("SoftAP started. IP="));
        log_.println(WiFi.softAPIP());
        logPortalState("event:AP_START");
        disablePortalPowerSave();
        break;
    case ARDUINO_EVENT_WIFI_AP_STOP:
        log_.println(F("SoftAP stopped."));
        logPortalState("event:AP_STOP");
        restorePortalPowerSave();
        break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
    {
        char mac[18] = {0};
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1], info.wifi_ap_staconnected.mac[2],
                 info.wifi_ap_staconnected.mac[3], info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
        log_.print(F("AP client connected: "));
        log_.print(mac);
        log_.print(F(" AID="));
        log_.println(info.wifi_ap_staconnected.aid);
        logPortalState("event:AP_STACONNECTED");
        break;
    }
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
    {
        char mac[18] = {0};
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1], info.wifi_ap_stadisconnected.mac[2],
                 info.wifi_ap_stadisconnected.mac[3], info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5]);
        log_.print(F("AP client disconnected: "));
        log_.print(mac);
        log_.print(F(" AID="));
        log_.println(info.wifi_ap_stadisconnected.aid);
        logPortalState("event:AP_STADISCONNECTED");
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
        logPortalState("event:STA_CONNECTED");
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
        logPortalState("event:STA_DISCONNECTED");
        lastStatus_ = WL_DISCONNECTED;
        hasLoggedIp_ = false;
        lastIp_ = IPAddress();
        {
            bool portalActive = manager_.getConfigPortalActive();
            bool onboarding = onboardingMode_ || !hasStoredCredentials();
            if (portalActive || onboarding)
            {
                log_.println(F("Ignoring STA disconnect while captive portal is active/onboarding."));
                pendingReconnect_ = false;
                lastReconnectAttemptMs_ = 0;
                waitingForIpSinceMs_ = 0;
                break;
            }
        }
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

void WiFiPortalService::logPortalState(const char *context)
{
    log_.print(F("Portal state"));
    if (context && context[0])
    {
        log_.print(F(" ("));
        log_.print(context);
        log_.print(F(")"));
    }
    log_.print(F(": status="));
    log_.print(static_cast<int>(WiFi.status()));
    log_.print(F(" configPortalActive="));
    log_.print(manager_.getConfigPortalActive() ? F("yes") : F("no"));
    log_.print(F(" webPortalActive="));
    log_.print(manager_.getWebPortalActive() ? F("yes") : F("no"));
    log_.print(F(" routesRegistered="));
    log_.print(routesRegistered_ ? F("yes") : F("no"));

    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) == ESP_OK)
    {
        log_.print(F(" mode="));
        switch (mode)
        {
        case WIFI_MODE_NULL:
            log_.print(F("NULL"));
            break;
        case WIFI_MODE_STA:
            log_.print(F("STA"));
            break;
        case WIFI_MODE_AP:
            log_.print(F("AP"));
            break;
        case WIFI_MODE_APSTA:
            log_.print(F("AP+STA"));
            break;
        default:
            log_.print(static_cast<int>(mode));
            break;
        }
    }

    IPAddress staIp = WiFi.localIP();
    IPAddress apIp = WiFi.softAPIP();
    log_.print(F(" staIP="));
    log_.print(staIp);
    log_.print(F(" apIP="));
    log_.print(apIp);
    log_.print(F(" apClients="));
    log_.print(WiFi.softAPgetStationNum());

    String ssid = WiFi.SSID();
    if (ssid.length())
    {
        log_.print(F(" ssid=\""));
        log_.print(ssid);
        log_.print(F("\""));
    }
    String apSsid = WiFi.softAPSSID();
    if (apSsid.length())
    {
        log_.print(F(" apSsid=\""));
        log_.print(apSsid);
        log_.print(F("\""));
    }
    log_.println();
}

bool WiFiPortalService::hasStoredCredentials() const
{
    wifi_config_t conf;
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK)
    {
        if (conf.sta.ssid[0] != 0)
            return true;
    }

    if (WiFi.SSID().length())
        return true;
    if (WiFi.psk().length())
        return true;
    return false;
}

void WiFiPortalService::prepareConfigPortalAp(const String &ssid)
{
    pendingReconnect_ = false;
    lastReconnectAttemptMs_ = 0;
    waitingForIpSinceMs_ = 0;
    log_.print(F("Preparing config portal AP for SSID '"));
    log_.print(ssid);
    log_.println(F("' (manager will start AP)."));
}

void WiFiPortalService::scheduleRestart(const char *reason)
{
    restartScheduled_ = true;
    restartAtMs_ = millis() + 1500;
    log_.print(F("Restart scheduled"));
    if (reason && reason[0])
    {
        log_.print(F(" ("));
        log_.print(reason);
        log_.print(F(")"));
    }
    log_.println(F("."));
}

void WiFiPortalService::disablePortalPowerSave()
{
    if (portalPsDisabled_)
        return;

    wifi_ps_type_t current;
    if (esp_wifi_get_ps(&current) == ESP_OK)
        previousPsType_ = current;

    if (esp_wifi_set_ps(WIFI_PS_NONE) == ESP_OK)
    {
        portalPsDisabled_ = true;
        log_.println("Wi-Fi power save disabled for captive portal.");
        esp_wifi_set_max_tx_power(78); // ~19.5 dBm
    }
}

void WiFiPortalService::restorePortalPowerSave()
{
    if (!portalPsDisabled_)
        return;

    if (esp_wifi_set_ps(previousPsType_) == ESP_OK)
    {
        log_.println("Wi-Fi power save restored.");
    }
    portalPsDisabled_ = false;
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

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

void WiFiPortalService::sendConfigBackupPage(const String &message)
{
    if (!manager_.server)
        return;

    bool isError = message.startsWith(F("ERROR:"));
    String display = isError ? message.substring(6) : message;
    display.trim();

    String noticeClass;
    if (!display.length())
    {
        noticeClass = "hidden";
    }
    else
    {
        noticeClass = isError ? "error" : "success";
    }

    TemplateReplacements vars = {
        {"{{NOTICE_CLASS}}", noticeClass},
        {"{{NOTICE_TEXT}}", htmlEscape(display)}};

    appendCommonTemplateVars(vars);
    sendTemplate("/portal/backup.html", vars);
}


void WiFiPortalService::handleConfigDownload()
{
    if (!manager_.server)
        return;

    auto &server = *manager_.server;
    String json = exportConfigJson();
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.sendHeader("Content-Disposition", "attachment; filename=\"radpro-wifi-bridge-config.json\"");
    server.send(200, "application/json", json);
}

void WiFiPortalService::handleConfigRestore()
{
    if (!manager_.server)
        return;

    auto &server = *manager_.server;
    String body;
    if (server.hasArg("configJson"))
        body = server.arg("configJson");
    else if (server.hasArg("plain"))
        body = server.arg("plain");

    body.trim();
    if (!body.length())
    {
        sendConfigBackupPage(F("ERROR: No configuration data received."));
        return;
    }

    String error;
    if (importConfigJson(body, error))
    {
        sendConfigBackupPage(F("Configuration restored. The bridge will reconnect with the imported settings."));

        // Close portal and reboot into the restored configuration for a clean start.
        if (manager_.getConfigPortalActive())
            manager_.stopConfigPortal();
        if (manager_.getWebPortalActive())
            manager_.stopWebPortal();

        onboardingMode_ = false;
        pendingReconnect_ = false;
        lastReconnectAttemptMs_ = 0;
        waitingForIpSinceMs_ = 0;
        hasLoggedIp_ = false;
        scheduleRestart("config restore");
    }
    else
    {
        sendConfigBackupPage(String(F("ERROR: ")) + error);
    }
}

String WiFiPortalService::exportConfigJson() const
{
    JsonDocument doc;
    doc["schema"] = "radpro-wifi-bridge-config";
    doc["bridgeFirmware"] = BRIDGE_FIRMWARE_VERSION;
    doc["generatedMs"] = millis();
    doc["deviceName"] = config_.deviceName;
    doc["mqttEnabled"] = config_.mqttEnabled;
    doc["mqttHost"] = config_.mqttHost;
    doc["mqttPort"] = config_.mqttPort;
    doc["mqttClient"] = config_.mqttClient;
    doc["mqttUser"] = config_.mqttUser;
    doc["mqttPassword"] = config_.mqttPassword;
    doc["mqttTopic"] = config_.mqttTopic;
    doc["mqttFullTopic"] = config_.mqttFullTopic;
    doc["readIntervalMs"] = config_.readIntervalMs;
    doc["openSenseMapEnabled"] = config_.openSenseMapEnabled;
    doc["openSenseBoxId"] = config_.openSenseBoxId;
    doc["openSenseApiKey"] = config_.openSenseApiKey;
    doc["openSenseTubeRateSensorId"] = config_.openSenseTubeRateSensorId;
    doc["openSenseDoseRateSensorId"] = config_.openSenseDoseRateSensorId;
    doc["gmcMapEnabled"] = config_.gmcMapEnabled;
    doc["gmcMapAccountId"] = config_.gmcMapAccountId;
    doc["gmcMapDeviceId"] = config_.gmcMapDeviceId;
    doc["radmonEnabled"] = config_.radmonEnabled;
    doc["radmonUser"] = config_.radmonUser;
    doc["radmonPassword"] = config_.radmonPassword;

    String json;
    serializeJsonPretty(doc, json);
    return json;
}

bool WiFiPortalService::importConfigJson(const String &body, String &errorMessage)
{
    errorMessage = String();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err)
    {
        errorMessage = String(F("Invalid JSON: ")) + err.c_str();
        return false;
    }

    AppConfig updated = config_;

    auto setString = [](String &target, JsonVariantConst value)
    {
        if (value.isNull())
            return;
        if (value.is<const char *>())
        {
            String s(value.as<const char *>());
            s.trim();
            target = s;
        }
        else if (value.is<String>())
        {
            String s = value.as<String>();
            s.trim();
            target = s;
        }
    };

    auto setBool = [](bool &target, JsonVariantConst value)
    {
        if (!value.isNull())
            target = value.as<bool>();
    };

    auto setUint16 = [](uint16_t &target, JsonVariantConst value)
    {
        if (value.isNull())
            return;
        uint32_t temp = value.as<uint32_t>();
        if (temp == 0 || temp > 65535)
            return;
        target = static_cast<uint16_t>(temp);
    };

    auto setUint32 = [](uint32_t &target, JsonVariantConst value)
    {
        if (value.isNull())
            return;
        uint32_t temp = value.as<uint32_t>();
        target = temp;
    };

    setString(updated.deviceName, doc["deviceName"]);
    setBool(updated.mqttEnabled, doc["mqttEnabled"]);
    setString(updated.mqttHost, doc["mqttHost"]);
    setUint16(updated.mqttPort, doc["mqttPort"]);
    setString(updated.mqttClient, doc["mqttClient"]);
    setString(updated.mqttUser, doc["mqttUser"]);
    setString(updated.mqttPassword, doc["mqttPassword"]);
    setString(updated.mqttTopic, doc["mqttTopic"]);
    setString(updated.mqttFullTopic, doc["mqttFullTopic"]);
    setUint32(updated.readIntervalMs, doc["readIntervalMs"]);
    setBool(updated.openSenseMapEnabled, doc["openSenseMapEnabled"]);
    setString(updated.openSenseBoxId, doc["openSenseBoxId"]);
    setString(updated.openSenseApiKey, doc["openSenseApiKey"]);
    setString(updated.openSenseTubeRateSensorId, doc["openSenseTubeRateSensorId"]);
    setString(updated.openSenseDoseRateSensorId, doc["openSenseDoseRateSensorId"]);
    setBool(updated.gmcMapEnabled, doc["gmcMapEnabled"]);
    setString(updated.gmcMapAccountId, doc["gmcMapAccountId"]);
    setString(updated.gmcMapDeviceId, doc["gmcMapDeviceId"]);
    setBool(updated.radmonEnabled, doc["radmonEnabled"]);
    setString(updated.radmonUser, doc["radmonUser"]);
    setString(updated.radmonPassword, doc["radmonPassword"]);

    if (updated.readIntervalMs < kMinReadIntervalMs)
        updated.readIntervalMs = kMinReadIntervalMs;

    if (!store_.save(updated))
    {
        errorMessage = F("Failed to save configuration to NVS.");
        led_.activateFault(FaultCode::NvsWriteFailure);
        return false;
    }

    config_ = updated;
    refreshParameters();
    led_.clearFault(FaultCode::NvsWriteFailure);
    pendingReconnect_ = true;
    lastReconnectAttemptMs_ = 0;
    hasLoggedIp_ = false;
    log_.println(F("Configuration restored from backup."));
    return true;
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

bool WiFiPortalService::readFile(const char *path, String &out)
{
    bool fsAvailable = true;
    File file = LittleFS.open(path, "r");
    if (!file)
    {
        fsAvailable = remountLittleFsIfNeeded(path);
        if (fsAvailable)
            file = LittleFS.open(path, "r");
    }
    if (!file)
    {
        log_.print(F("Missing portal asset: "));
        log_.println(path);
        if (fsAvailable)
        {
            log_.print(F("LittleFS.exists? "));
            log_.println(LittleFS.exists(path) ? F("yes") : F("no"));
            dumpFilesystemContents(F("on missing asset"));
        }
        else
        {
            log_.println(F("LittleFS unavailable; cannot enumerate assets."));
        }
        return false;
    }

    log_.print(F("Serving asset: "));
    log_.print(path);
    log_.print(F(" size="));
    log_.println(file.size());

    out.clear();
    out.reserve(file.size() + 8);
    while (file.available())
    {
        out += static_cast<char>(file.read());
    }
    file.close();
    return true;
}

bool WiFiPortalService::sendStaticFile(const char *path, const char *contentType)
{
    if (!manager_.server)
        return false;

    File file = LittleFS.open(path, "r");
    if (!file)
    {
        if (!remountLittleFsIfNeeded(path))
            return false;
        file = LittleFS.open(path, "r");
    }

    if (!file)
    {
        log_.print(F("Missing portal asset: "));
        log_.println(path);
        log_.print(F("LittleFS.exists? "));
        log_.println(LittleFS.exists(path) ? F("yes") : F("no"));
        return false;
    }

    log_.print(F("Serving asset: "));
    log_.print(path);
    log_.print(F(" size="));
    log_.println(file.size());

    size_t sent = manager_.server->streamFile(file, contentType);
    file.close();

    if (sent == 0)
    {
        log_.print(F("Failed to stream asset: "));
        log_.println(path);
        return false;
    }

    log_.print(F("Served asset: "));
    log_.print(path);
    log_.print(F(" bytes="));
    log_.println(sent);
    return true;
}

void WiFiPortalService::applyTemplateReplacements(String &content, const TemplateReplacements &replacements)
{
    for (const auto &entry : replacements)
    {
        content.replace(entry.first, entry.second);
    }
}

bool WiFiPortalService::sendTemplate(const char *path, const TemplateReplacements &replacements)
{
    if (!manager_.server)
    {
        log_.print(F("Cannot send template "));
        log_.print(path);
        log_.println(F(": web server not ready."));
        return false;
    }

    String content;
    if (!readFile(path, content))
    {
        log_.print(F("Retrying template load: "));
        log_.println(path);
        // Attempt to reload the menu snippet in case LittleFS mounted late.
        if (strcmp(path, "/portal/menu.html") == 0)
        {
            menuHtml_ = String();
            ensureMenuHtmlLoaded();
            content = menuHtml_;
        }

        if (!content.length())
        {
            sendTemplateError(path);
            return false;
        }
    }

    applyTemplateReplacements(content, replacements);

    manager_.server->send(200, "text/html", content);
    log_.print(F("Served template OK: "));
    log_.println(path);
    return true;
}

String WiFiPortalService::resolvePortalLocale() const
{
    DeviceInfoSnapshot snap = deviceInfo_.snapshot();
    String locale = snap.locale;
    locale.trim();
    locale.toLowerCase();
    if (locale.startsWith("de"))
        return "de";
    return "en";
}

void WiFiPortalService::appendCommonTemplateVars(TemplateReplacements &replacements)
{
    String locale = resolvePortalLocale();
    replacements.emplace_back("{{LOCALE}}", locale);
}

void WiFiPortalService::handleLogsJson()
{
    if (!manager_.server)
        return;

    std::vector<DebugLogEntry> entries;
    log_.copyEntries(entries);

    JsonDocument doc;
    JsonArray lines = doc["lines"].to<JsonArray>();
    for (const auto &entry : entries)
    {
        lines.add(entry.text);
        if (doc.overflowed())
            break;
    }
    doc["count"] = static_cast<uint32_t>(entries.size());
    doc["latest"] = log_.latestId();

    String body;
    serializeJson(doc, body);
    manager_.server->send(200, "application/json", body);
}

void WiFiPortalService::sendTemplateError(const char *path)
{
    if (!manager_.server)
    {
        log_.print(F("Cannot send template error for "));
        log_.print(path);
        log_.println(F(" because server is null."));
        return;
    }
    String body = String(F("Template not found: ")) + path;
    manager_.server->send(500, "text/plain", body);
    log_.print(F("Sent template error response for "));
    log_.println(path);
}

bool WiFiPortalService::remountLittleFsIfNeeded(const char *context)
{
    log_.print(F("LittleFS unavailable while accessing "));
    log_.print(context);
    log_.println(F("; attempting remount."));
    if (LittleFS.begin(false, BridgeFileSystem::kBasePath, BridgeFileSystem::kMaxFiles, BridgeFileSystem::kLabel))
    {
        log_.println(F("LittleFS remount successful."));
        dumpFilesystemContents(F("after remount"));
        return true;
    }
    log_.println(F("LittleFS remount failed."));
    return false;
}

void WiFiPortalService::dumpFilesystemContents(const __FlashStringHelper *reason)
{
    BridgeFileSystem::dumpTree(log_, reason);
}

void WiFiPortalService::ensureMenuHtmlLoaded()
{
    log_.println(F("ensureMenuHtmlLoaded(): checking cached menu HTML…"));
    if (menuHtml_.length())
    {
        log_.println(F("Menu HTML already loaded."));
        return;
    }
    log_.print(F("ensureMenuHtmlLoaded(): loading /portal/menu.html (exists? "));
    log_.print(LittleFS.exists("/portal/menu.html") ? F("yes") : F("no"));
    log_.println(F(")"));
    if (!readFile("/portal/menu.html", menuHtml_))
    {
        log_.println(F("Failed to load /portal/menu.html"));
    }
    else
    {
        log_.print(F("Loaded menu HTML ("));
        log_.print(menuHtml_.length());
        log_.println(F(" bytes)."));
        if (paramsAttached_)
        {
            log_.println(F("Re-applying custom menu HTML to WiFiManager."));
            applyMenuHtmlForLocale(resolvePortalLocale());
        }
    }
}

void WiFiPortalService::applyMenuHtmlForLocale(const String &locale)
{
    if (!menuHtml_.length())
        return;

    if (menuHtmlLocale_ == locale && menuHtmlRendered_.length())
    {
        manager_.setCustomMenuHTML(menuHtmlRendered_.c_str());
        return;
    }

    menuHtmlRendered_ = menuHtml_;
    menuHtmlRendered_.replace("{{PORTAL_LOCALE}}", locale);
    menuHtmlLocale_ = locale;

    manager_.setCustomMenuHTML(menuHtmlRendered_.c_str());
}
