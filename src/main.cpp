#include <Arduino.h>
#include "UsbCdcHost.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include <cstring>
#include <utility>
#include <vector>
#include <functional>
#include <WiFi.h>
#include <LittleFS.h>
extern "C"
{
#include "esp_littlefs.h"
}
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "DeviceManager.h"
#include "AppConfig/AppConfig.h"
#include "ConfigPortal/WiFiPortalService.h"
#include "Led/LedController.h"
#include "Mqtt/MqttPublisher.h"
#include "OpenSenseMap/OpenSenseMapPublisher.h"
#include "GmcMap/GmcMapPublisher.h"
#include "Radmon/RadmonPublisher.h"
#include "BridgeDiagnostics.h"
#include "DeviceInfo/DeviceInfoStore.h"

#ifndef BRIDGE_FIRMWARE_VERSION
#define BRIDGE_FIRMWARE_VERSION "0.0.0"
#endif

// =========================
// Board / LED definitions
// =========================
#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48 // ESP32-S3 DevKitC-1 onboard WS2812 pin
#endif

// Prefer the CP210x/UART "second USB port" for debug when USB-CDC is on
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
#define DBG Serial0
#else
#define DBG Serial
#endif

// =========================
// Startup configuration
// =========================
#define INITIAL_STARTUP_DELAY_MS 0
#define ALLOW_EARLY_START true

// Program state
static bool isRunning = false;
static unsigned long startupDelayMs = INITIAL_STARTUP_DELAY_MS;
static unsigned long startupStartTime = 0;
// Forward declarations
static void handleStartupLogic();
static void runMainLogic();

// =========================
// USB Host wrapper
// =========================
static UsbCdcHost usb;
static DeviceManager device_manager(usb);
static const std::vector<std::pair<uint16_t, uint16_t>> kSupportedUsbVidPid = {
    {0x1A86, 0x7523}, // CH340/341 (Bosean FS-600)
    {0x1A86, 0x7522}, // Alternate CH340 PID sometimes reported
    {0x1A86, 0x5523}, // CH341 variant
    {0x1A86, 0x55D4}, // CH9102F (Fnirsi GC01)
    {0x1A86, 0x55D3}  // CH9102X (Fnirsi GC01 alt)
};

static LedController ledController;
static BridgeDiagnostics diagnostics(DBG, ledController);
static DeviceInfoStore deviceInfoStore;

static const char *kLittleFsBasePath = "/littlefs";
static const char *kLittleFsLabel = "spiffs";
static AppConfig appConfig;
static AppConfigStore configStore;
static WiFiPortalService portalService(appConfig, configStore, deviceInfoStore, DBG, ledController);
static MqttPublisher mqttPublisher(appConfig, DBG, ledController);
static OpenSenseMapPublisher openSenseMapPublisher(appConfig, DBG, BRIDGE_FIRMWARE_VERSION);
static GmcMapPublisher gmcMapPublisher(appConfig, DBG, BRIDGE_FIRMWARE_VERSION);
static RadmonPublisher radmonPublisher(appConfig, DBG, BRIDGE_FIRMWARE_VERSION);
static bool deviceReady = false;
static bool deviceError = false;
static bool mqttError = false;

static void logLittleFsStats(const char *stage)
{
    size_t total = 0;
    size_t used = 0;
    esp_err_t err = esp_littlefs_info(kLittleFsLabel, &total, &used);
    DBG.print("[LittleFS] info (");
    DBG.print(stage);
    DBG.print("): err=");
    DBG.println(esp_err_to_name(err));
    if (err == ESP_OK)
    {
        DBG.print("[LittleFS] total=");
        DBG.print(total);
        DBG.print(" bytes used=");
        DBG.println(used);
    }
}

static void dumpLittleFsTree(const char *reason)
{
    DBG.print("[LittleFS] Directory listing (");
    DBG.print(reason);
    DBG.println("):");

    File root = LittleFS.open("/");
    if (!root || !root.isDirectory())
    {
        DBG.println("[LittleFS] <root unavailable>");
        return;
    }

    std::function<void(const String &, File &)> dumpDir = [&](const String &path, File &dir) {
        File entry = dir.openNextFile();
        while (entry)
        {
            String entryPath = path + entry.name();
            DBG.print("  ");
            DBG.print(entryPath);
            if (entry.isDirectory())
            {
                DBG.println("/ (dir)");
                File sub = LittleFS.open(entryPath);
                if (sub)
                {
                    dumpDir(entryPath + "/", sub);
                    sub.close();
                }
            }
            else
            {
                DBG.print(" size=");
                DBG.println(entry.size());
            }
            entry = dir.openNextFile();
        }
    };

    dumpDir(String("/"), root);
    root.close();
}

static bool mountLittleFs(const char *stage, bool formatOnFail)
{
    DBG.print("[LittleFS] mount request (");
    DBG.print(stage);
    DBG.println(")");
    DBG.print("[LittleFS] already mounted? ");
    DBG.println(esp_littlefs_mounted(kLittleFsLabel) ? "yes" : "no");

    bool mounted = LittleFS.begin(formatOnFail, kLittleFsBasePath, 10, kLittleFsLabel);
    DBG.print("[LittleFS] begin returned ");
    DBG.println(mounted ? "true" : "false");
    DBG.print("[LittleFS] mounted after begin? ");
    DBG.println(esp_littlefs_mounted(kLittleFsLabel) ? "yes" : "no");

    logLittleFsStats(stage);
    if (mounted)
        dumpLittleFsTree(stage);
    return mounted;
}

// =========================
// Arduino setup / loop
// =========================
void setup()
{
    // Bring up both Serial endpoints to be safe
    Serial.begin(115200);
    DBG.begin(115200);
    delay(300);

    DBG.println("Initializing RadPro WiFi Bridge…");

    if (!mountLittleFs("setup-initial", true))
    {
        DBG.println("[LittleFS] Initial mount failed; portal assets unavailable.");
    }
    else
    {
        DBG.print("[LittleFS] Portal menu present: ");
        DBG.println(LittleFS.exists("/portal/menu.html") ? "yes" : "no");
        DBG.print("[LittleFS] MQTT page present: ");
        DBG.println(LittleFS.exists("/portal/mqtt.html") ? "yes" : "no");
    }

    deviceInfoStore.setBridgeFirmware(BRIDGE_FIRMWARE_VERSION);

    esp_reset_reason_t resetReason = esp_reset_reason();
    if (resetReason == ESP_RST_BROWNOUT)
    {
        ledController.activateFault(FaultCode::PowerBrownout);
    }
    else if (resetReason == ESP_RST_TASK_WDT || resetReason == ESP_RST_WDT)
    {
        ledController.activateFault(FaultCode::WatchdogReset);
    }

    ledController.begin();
    ledController.setMode(LedMode::Booting);
    ledController.update();

    diagnostics.initialize();

    // Record start time for non-blocking startup delay
    startupStartTime = millis();

    device_manager.setLineHandler([&](const String &line)
                                  { diagnostics.handleLine(line); });
    device_manager.setRawHandler([&](const uint8_t *data, size_t len)
                                 { diagnostics.handleRaw(data, len); });
    device_manager.setCommandResultHandler([&](DeviceManager::CommandType type, const String &value, bool success)
                                           {
        if (!success)
        {
            bool transient = (type == DeviceManager::CommandType::TubePulseCount || type == DeviceManager::CommandType::TubeRate);
            if (!transient)
            {
                deviceError = true;
                if (type == DeviceManager::CommandType::DeviceId)
                    deviceReady = false;
                DBG.print("Device command failed: ");
                DBG.println(static_cast<int>(type));
                ledController.triggerPulse(LedPulse::MqttFailure, 250);
                if (type == DeviceManager::CommandType::DeviceId)
                    ledController.activateFault(FaultCode::DeviceIdTimeout);
                else
                    ledController.activateFault(FaultCode::CommandTimeout);
            }
            return;
        }

        deviceError = false;
        if (type == DeviceManager::CommandType::DeviceId)
        {
            deviceReady = value.length() > 0;
            if (deviceReady)
                ledController.clearFault(FaultCode::DeviceIdTimeout);
        }

        if (type == DeviceManager::CommandType::DeviceSensitivity)
        {
            float sens = value.toFloat();
            if (sens > 0.0f)
                ledController.clearFault(FaultCode::MissingSensitivity);
            else
                ledController.activateFault(FaultCode::MissingSensitivity);
        }

        if (type == DeviceManager::CommandType::TubeDoseRate)
        {
            if (!device_manager.hasSensitivity())
                ledController.activateFault(FaultCode::MissingSensitivity);
            else
                ledController.clearFault(FaultCode::MissingSensitivity);
        }

        ledController.clearFault(FaultCode::CommandTimeout);

        deviceInfoStore.update(type, value);
        mqttPublisher.onCommandResult(type, value);
        openSenseMapPublisher.onCommandResult(type, value);
        gmcMapPublisher.onCommandResult(type, value);
        radmonPublisher.onCommandResult(type, value); });
    device_manager.begin(kSupportedUsbVidPid);

    // Optionally set target baud for CDC device
    // usb.setBaud(115200);

    // Start USB host + CDC listener + TX task
    if (!usb.begin())
    {
        DBG.println("ERROR: usb.begin() failed");
        ledController.activateFault(FaultCode::UsbInterfaceFailure);
    }
    else
    {
        DBG.println("usb.begin() OK");
        ledController.clearFault(FaultCode::UsbInterfaceFailure);
        if (ALLOW_EARLY_START)
        {
            DBG.println("Send 'start', 'delay <ms>', or 'raw on/off/toggle' on this port.");
        }
    }

    if (!configStore.load(appConfig))
    {
        DBG.println("Preferences read failed; keeping defaults.");
        ledController.activateFault(FaultCode::NvsLoadFailure);
    }
    else
    {
        ledController.clearFault(FaultCode::NvsLoadFailure);
    }

    portalService.begin();
    mqttPublisher.begin();
    mqttPublisher.setBridgeVersion(BRIDGE_FIRMWARE_VERSION);
    openSenseMapPublisher.begin();
    gmcMapPublisher.begin();
    radmonPublisher.begin();
    mqttPublisher.setPublishCallback([&](bool success)
                                     {
        if (success)
        {
            mqttError = false;
            ledController.triggerPulse(LedPulse::MqttSuccess, 150);
            ledController.clearFault(FaultCode::MqttConnectionReset);
        }
        else
        {
            mqttError = true;
            DBG.println("MQTT publish failed.");
            ledController.triggerPulse(LedPulse::MqttFailure, 250);
            ledController.activateFault(FaultCode::MqttConnectionReset);
        } });

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(appConfig.deviceName.c_str());

    if (!portalService.connect(false))
    {
        DBG.println("Auto-connect or portal timed out; starting configuration portal.");
        portalService.connect(true);
    }
    mqttPublisher.updateConfig();
    openSenseMapPublisher.updateConfig();
    gmcMapPublisher.updateConfig();
    radmonPublisher.updateConfig();
    portalService.maintain();
    diagnostics.updateLedStatus(isRunning, deviceError, mqttError, deviceReady);
    ledController.update();
}

void loop()
{
    if (!isRunning)
    {
        handleStartupLogic();
    }
    else
    {
        runMainLogic();
    }

    device_manager.loop();
    portalService.syncIfRequested();
    portalService.maintain();
    portalService.process();
    mqttPublisher.updateConfig();
    mqttPublisher.loop();
    openSenseMapPublisher.updateConfig();
    openSenseMapPublisher.loop();
    gmcMapPublisher.updateConfig();
    gmcMapPublisher.loop();
    radmonPublisher.updateConfig();
    radmonPublisher.loop();
    if (!usb.isConnected())
        deviceReady = false;
    diagnostics.updateLedStatus(isRunning, deviceError, mqttError, deviceReady);
    ledController.update();

    // Keep loop snappy; USB host runs in its own tasks
    delay(5);
}

// =========================
// Startup state machine
// =========================
static void handleStartupLogic()
{
    if (!isRunning)
        ledController.setMode(LedMode::WaitingForStart);

    // 1) Timer elapsed?
    if (millis() - startupStartTime >= startupDelayMs)
    {
        isRunning = true;
    }

    // 2) User commands on DBG (e.g., CP210x UART)
    if (DBG.available() > 0)
    {
        String command = DBG.readStringUntil('\n');
        command.trim();

        if (command.startsWith("delay "))
        {
            long newDelay = command.substring(6).toInt();
            if (newDelay > 0)
            {
                startupDelayMs = (unsigned long)newDelay;
                startupStartTime = millis(); // reset timer
                DBG.print("Startup delay updated to: ");
                DBG.print(startupDelayMs);
                DBG.println(" ms");
            }
        }
        else if (command.equalsIgnoreCase("raw on"))
        {
            device_manager.setRawLogging(true);
            DBG.println("USB raw logging enabled.");
        }
        else if (command.equalsIgnoreCase("raw off"))
        {
            device_manager.setRawLogging(false);
            DBG.println("USB raw logging disabled.");
        }
        else if (command.equalsIgnoreCase("raw toggle"))
        {
            device_manager.toggleRawLogging();
            DBG.print("USB raw logging toggled ");
            DBG.println(device_manager.rawLoggingEnabled() ? "ON." : "OFF.");
        }
        else if (command.equalsIgnoreCase("usb debug on"))
        {
            diagnostics.setUsbDebugEnabled(true);
        }
        else if (command.equalsIgnoreCase("usb debug off"))
        {
            diagnostics.setUsbDebugEnabled(false);
        }
        else if (command.equalsIgnoreCase("usb debug toggle"))
        {
            diagnostics.toggleUsbDebug();
        }
        else if (ALLOW_EARLY_START && command.length() > 0)
        {
            isRunning = true;
            DBG.println("Early start triggered by user!");
        }
    }

    // 3) If starting now, announce and switch LED to a short "go" blink
    if (isRunning)
    {
        DBG.println("Starting RadPro WiFi Bridge…");
        device_manager.start();
        portalService.enableStatusLogging();
        ledController.triggerPulse(LedPulse::MqttSuccess, 200);
        return;
    }

    // Periodic countdown message once per second
    static unsigned long lastCountdownTime = 0;
    if (millis() - lastCountdownTime >= 1000)
    {
        lastCountdownTime = millis();
        unsigned long remaining = (startupDelayMs - (millis() - startupStartTime)) / 1000;
        DBG.print("Starting in ");
        DBG.print(remaining);
        DBG.println(" seconds…");
    }
}

// =========================
// Main application tasks
// =========================
static void runMainLogic()
{
    // Poll rad-pro statistics according to configured interval
    static unsigned long lastStatsRequest = 0;
    unsigned long now = millis();
    uint32_t interval = appConfig.readIntervalMs;
    if (interval < kMinReadIntervalMs)
        interval = kMinReadIntervalMs;
    if (now - lastStatsRequest >= interval)
    {
        lastStatsRequest = now;
        device_manager.requestStats();
    }
}
