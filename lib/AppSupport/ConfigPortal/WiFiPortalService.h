#pragma once

#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_wifi_types.h>
#include <vector>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "DeviceInfo/DeviceInfoStore.h"
#include "DeviceInfo/DeviceInfoPage.h"
#include "DeviceInfo/BridgeInfoPage.h"
#include "AppConfig/AppConfig.h"
#include "Led/LedController.h"
#include "Ota/OtaUpdateService.h"
#include "Logging/DebugLogStream.h"

class WiFiPortalService
{
public:
    using TemplateReplacements = std::vector<std::pair<String, String>>;

    WiFiPortalService(AppConfig &config, AppConfigStore &store, DeviceInfoStore &info, DebugLogStream &logPort, LedController &led);

    void begin();
    bool connect(bool forcePortal);
    void maintain();
    void process();
    void syncIfRequested();
    void dumpStatus();
    void enableStatusLogging();
    void setOtaStartCallback(std::function<void()> cb);

private:
    friend class MqttPublisher;
    friend class OpenSenseMapPublisher;
    friend class RadmonPublisher;
    friend class GmcMapPublisher;

    void refreshParameters();
    void attachParameters();
    bool applyFromParameters(bool persist, bool forceSave = false);
    void handleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    void logStatusIfNeeded();
    void logConnectionDetails(const IPAddress &ip, const IPAddress &gateway, const IPAddress &mask);
    void logStatus();
    void attemptReconnect();
    void sendConfigBackupPage(const String &message = String());
    void handleConfigDownload();
    void handleConfigRestore();
    String exportConfigJson() const;
    bool importConfigJson(const String &body, String &errorMessage);
    static String htmlEscape(const String &value);
    void sendOtaPage(const String &message = String());
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
    void handleOtaStatus();
    void handleOtaFetch();
    void handleOtaUploadBegin();
    void handleOtaUploadPartBegin();
    void handleOtaUploadPartChunk();
    void handleOtaUploadPartFinish();
    void handleOtaUploadFinish();
    void handleOtaCancel();
    struct ManifestPart
    {
        String path;
        uint32_t offset = 0;
    };
    bool parseManifestParts(const String &manifestJson, std::vector<ManifestPart> &parts, String &version, String &error);
    bool fetchRemoteManifest(String &manifestJson, String &version, String &error);
    bool downloadRemotePart(const String &baseUrl, const String &path, uint32_t offset, String &error);
    void resetOtaProgress();
    void setOtaProgress(const String &message, size_t totalBytes, size_t writtenBytes);
    void updateOtaBytes(size_t writtenBytes);
    void setOtaMessage(const String &message);
    bool decodeBase64Chunk(const String &input, std::vector<uint8_t> &out, String &error);
    void notifyOtaStart();
    void refreshLatestRemoteVersion(bool force);
    static void otaTaskThunk(void *param);
    void runRemoteFetchTask();
    static void manifestTaskThunk(void *param);
    void runManifestFetchTask(bool force);
    void sendJson(int code, const String &body);

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
    OtaUpdateService otaService_;
    bool otaUploadOk_ = true;
    String otaProgressMessage_;
    size_t otaBytesExpected_ = 0;
    size_t otaBytesWritten_ = 0;
    unsigned long otaLastProgressMs_ = 0;
    String latestRemoteVersion_;
    String latestRemoteError_;
    unsigned long latestRemoteCheckMs_ = 0;
    std::vector<uint8_t> otaChunkBuffer_;
    TaskHandle_t otaTaskHandle_ = nullptr;
    portMUX_TYPE otaLock_ = portMUX_INITIALIZER_UNLOCKED;
    TaskHandle_t manifestTaskHandle_ = nullptr;
    bool manifestForceRefresh_ = false;
    std::function<void()> onOtaStart_;
    bool otaHooksFired_ = false;
};
