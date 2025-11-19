#pragma once

#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_wifi_types.h>
#include <vector>
#include "DeviceInfo/DeviceInfoStore.h"
#include "DeviceInfo/DeviceInfoPage.h"
#include "DeviceInfo/BridgeInfoPage.h"
#include "AppConfig/AppConfig.h"
#include "Led/LedController.h"
#include "Logging/DebugLogStream.h"

class WiFiPortalService
{
public:
    WiFiPortalService(AppConfig &config, AppConfigStore &store, DeviceInfoStore &info, DebugLogStream &logPort, LedController &led);

    void begin();
    bool connect(bool forcePortal);
    void maintain();
    void process();
    void syncIfRequested();
    void dumpStatus();
    void enableStatusLogging();

private:
    void refreshParameters();
    void attachParameters();
    bool applyFromParameters(bool persist, bool forceSave = false);
    void handleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    void logStatusIfNeeded();
    void logConnectionDetails(const IPAddress &ip, const IPAddress &gateway, const IPAddress &mask);
    void logStatus();
    void attemptReconnect();
    void sendMqttForm(const String &message = String());
    void sendOpenSenseForm(const String &message = String());
    void sendRadmonForm(const String &message = String());
    void sendGmcMapForm(const String &message = String());
    void handleMqttPost();
    void handleOpenSensePost();
    void handleRadmonPost();
    void handleGmcMapPost();
    void sendConfigBackupPage(const String &message = String());
    void handleConfigDownload();
    void handleConfigRestore();
    String exportConfigJson() const;
    bool importConfigJson(const String &body, String &errorMessage);
    static String htmlEscape(const String &value);
    using TemplateReplacements = std::vector<std::pair<String, String>>;
    bool sendTemplate(const char *path, const TemplateReplacements &replacements);
    bool readFile(const char *path, String &out);
    void sendTemplateError(const char *path);
    void ensureMenuHtmlLoaded();
    void applyMenuHtmlForLocale(const String &locale);
    bool remountLittleFsIfNeeded(const char *context);
    void dumpFilesystemContents(const __FlashStringHelper *reason);
    bool sendStaticFile(const char *path, const char *contentType);
    void applyTemplateReplacements(String &content, const TemplateReplacements &replacements);
    void appendCommonTemplateVars(TemplateReplacements &replacements);
    void handleLogsJson();
    void disablePortalPowerSave();
    void restorePortalPowerSave();
    void logPortalState(const char *context);
    bool hasStoredCredentials() const;
    void scheduleRestart(const char *reason);
    String resolvePortalLocale() const;

    AppConfig &config_;
    AppConfigStore &store_;
    DeviceInfoStore &deviceInfo_;
    DeviceInfoPage deviceInfoPage_;
    BridgeInfoPage bridgeInfoPage_;
    WiFiManager manager_;
    DebugLogStream &log_;
    LedController &led_;

    WiFiManagerParameter paramDeviceName_;
    WiFiManagerParameter paramMqttHost_;
    WiFiManagerParameter paramMqttPort_;
    WiFiManagerParameter paramMqttClient_;
    WiFiManagerParameter paramMqttUser_;
    WiFiManagerParameter paramMqttPass_;
    WiFiManagerParameter paramMqttTopic_;
    WiFiManagerParameter paramMqttFullTopic_;
    WiFiManagerParameter paramReadInterval_;
    WiFiManagerParameter paramGmcAccount_;
    WiFiManagerParameter paramGmcDevice_;
    WiFiManagerParameter paramRadmonUser_;
    WiFiManagerParameter paramRadmonPassword_;
    bool paramsAttached_;
    String menuHtml_;
    String menuHtmlRendered_;
    String menuHtmlLocale_;
    wl_status_t lastStatus_;
    WiFiEventId_t wifiEventId_;
    IPAddress lastIp_;
    bool hasLoggedIp_;
    bool loggingEnabled_ = false;
    bool pendingReconnect_ = false;
    unsigned long lastReconnectAttemptMs_ = 0;
    unsigned long waitingForIpSinceMs_ = 0;
    String lastKnownSsid_;
    String lastKnownPass_;
    bool portalPsDisabled_ = false;
    wifi_ps_type_t previousPsType_ = WIFI_PS_MIN_MODEM;
    bool routesRegistered_ = false;
    bool restartScheduled_ = false;
    unsigned long restartAtMs_ = 0;
};
