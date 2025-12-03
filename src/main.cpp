#include <Arduino.h>
#include "UsbCdcHost.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#if defined(__has_include)
#if __has_include(<esp_app_desc.h>)
#include <esp_app_desc.h>
#define HAS_ESP_APP_DESC 1
#endif
#endif
#include <cstring>
#include <utility>
#include <vector>
#include <WiFi.h>
#include <LittleFS.h>
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
#include "PeripheralStarter.h"
#include "DeviceInfo/DeviceInfoStore.h"
#include "Ota/OtaUpdateService.h"
#include "FileSystem/BridgeFileSystem.h"
#include "Logging/DebugLogStream.h"

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
#define DBG_SERIAL Serial0
#else
#define DBG_SERIAL Serial
#endif

static DebugLogStream debugSerial(DBG_SERIAL);
#define DBG debugSerial

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
static const char *commandTypeName(DeviceManager::CommandType type);
static const char *ledModeName(LedMode mode);

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
static bool lastDeviceReadyLogged = false;

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
static bool updateInProgress = false;
static PeripheralStarter peripheralStarter(device_manager, usb, mqttPublisher, openSenseMapPublisher, gmcMapPublisher, radmonPublisher, ledController, DBG, ALLOW_EARLY_START, BRIDGE_FIRMWARE_VERSION);
static LedMode lastLoggedMode = LedMode::Booting;

// =========================
// Arduino setup / loop
// =========================
void setup()
{
    // Bring up both Serial endpoints to be safe
    Serial.begin(115200);
    DBG.begin(115200);
    delay(300);

    // Confirm current app as valid to cancel any pending rollback.
    esp_ota_mark_app_valid_cancel_rollback();

    DBG.println("Initializing RadPro WiFi Bridge…");

    if (!BridgeFileSystem::mount(DBG, "setup-initial", true))
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
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    if (running && boot)
    {
        DBG.print("Partition (running): label=");
        DBG.print(running->label);
        DBG.print(" addr=0x");
        DBG.print(running->address, HEX);
        DBG.print(" size=");
        DBG.println(running->size);

        DBG.print("Partition (boot): label=");
        DBG.print(boot->label);
        DBG.print(" addr=0x");
        DBG.print(boot->address, HEX);
        DBG.print(" size=");
        DBG.println(boot->size);
    }
#ifdef HAS_ESP_APP_DESC
    if (const esp_app_desc_t *desc = esp_app_get_description())
    {
        DBG.print("App description: ");
        DBG.print(desc->project_name);
        DBG.print(" v");
        DBG.print(desc->version);
        DBG.print(" built ");
        DBG.print(desc->date);
        DBG.print(" ");
        DBG.println(desc->time);
    }
#endif

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

    device_manager.setLineHandler([&](const String &line) { diagnostics.handleLine(line); });
    device_manager.setRawHandler([&](const uint8_t *data, size_t len) { diagnostics.handleRaw(data, len); });
    device_manager.setCommandResultHandler([&](DeviceManager::CommandType type, const String &value, bool success) {
        if (!success)
        {
            DBG.print("Device command failed: ");
            DBG.print(commandTypeName(type));
            DBG.print(" (");
            DBG.print(static_cast<int>(type));
            DBG.println(")");
            bool transient = (type == DeviceManager::CommandType::TubePulseCount ||
                               type == DeviceManager::CommandType::TubeRate ||
                               type == DeviceManager::CommandType::DeviceBatteryVoltage ||
                               type == DeviceManager::CommandType::DeviceBatteryPercent ||
                               (type == DeviceManager::CommandType::DeviceId && !deviceReady));
            if (!transient)
            {
                deviceError = true;
                if (type == DeviceManager::CommandType::DeviceId)
                {
                    deviceReady = false;
                    DBG.println("DeviceReady cleared after DeviceId failure.");
                }
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
            {
                ledController.clearFault(FaultCode::DeviceIdTimeout);
                if (!lastDeviceReadyLogged)
                {
                    DBG.println("DeviceReady set after DeviceId response.");
                }
            }
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
        radmonPublisher.onCommandResult(type, value);
    });

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
    portalService.setOtaStartCallback([&]()
                                      { OtaUpdateService::EnterUpdateMode(device_manager, usb, mqttPublisher, openSenseMapPublisher, gmcMapPublisher, radmonPublisher, updateInProgress); });
    peripheralStarter.startIfNeeded(WiFi.status() == WL_CONNECTED, kSupportedUsbVidPid);
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
    LedMode currentMode = ledController.currentModeForDebug();
    if (currentMode != lastLoggedMode)
    {
        DBG.print("LED mode -> ");
        DBG.println(ledModeName(currentMode));
        lastLoggedMode = currentMode;
    }
    ledController.update();
}

void loop()
{
    peripheralStarter.startIfNeeded(WiFi.status() == WL_CONNECTED, kSupportedUsbVidPid);

    if (!isRunning)
    {
        handleStartupLogic();
    }
    else
    {
        runMainLogic();
    }

    if (!updateInProgress && peripheralStarter.started())
    {
        device_manager.loop();
    }
    portalService.syncIfRequested();
    portalService.maintain();
    portalService.process();
    if (!updateInProgress && peripheralStarter.started())
    {
        mqttPublisher.updateConfig();
        mqttPublisher.loop();
        openSenseMapPublisher.updateConfig();
        openSenseMapPublisher.loop();
        gmcMapPublisher.updateConfig();
        gmcMapPublisher.loop();
        radmonPublisher.updateConfig();
        radmonPublisher.loop();
    }
    if (!usb.isConnected())
    {
        if (deviceReady)
        {
            DBG.println("DeviceReady cleared: USB disconnected.");
        }
        deviceReady = false;
        lastDeviceReadyLogged = false;
    }
    else if (deviceReady)
    {
        lastDeviceReadyLogged = true;
    }
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
    if (!peripheralStarter.started())
    {
        ledController.setMode(LedMode::WaitingForStart);
        return;
    }

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

static const char *commandTypeName(DeviceManager::CommandType type)
{
    switch (type)
    {
    case DeviceManager::CommandType::DeviceId:
        return "DeviceId";
    case DeviceManager::CommandType::DeviceModel:
        return "DeviceModel";
    case DeviceManager::CommandType::DeviceFirmware:
        return "DeviceFirmware";
    case DeviceManager::CommandType::DeviceLocale:
        return "DeviceLocale";
    case DeviceManager::CommandType::DevicePower:
        return "DevicePower";
    case DeviceManager::CommandType::DeviceBatteryVoltage:
        return "DeviceBatteryVoltage";
    case DeviceManager::CommandType::DeviceBatteryPercent:
        return "DeviceBatteryPercent";
    case DeviceManager::CommandType::DeviceTime:
        return "DeviceTime";
    case DeviceManager::CommandType::DeviceTimeZone:
        return "DeviceTimeZone";
    case DeviceManager::CommandType::DeviceSensitivity:
        return "DeviceSensitivity";
    case DeviceManager::CommandType::TubeTime:
        return "TubeTime";
    case DeviceManager::CommandType::TubePulseCount:
        return "TubePulseCount";
    case DeviceManager::CommandType::TubeRate:
        return "TubeRate";
    case DeviceManager::CommandType::TubeDoseRate:
        return "TubeDoseRate";
    case DeviceManager::CommandType::TubeDeadTime:
        return "TubeDeadTime";
    case DeviceManager::CommandType::TubeDeadTimeCompensation:
        return "TubeDeadTimeCompensation";
    case DeviceManager::CommandType::TubeHVFrequency:
        return "TubeHVFrequency";
    case DeviceManager::CommandType::TubeHVDutyCycle:
        return "TubeHVDutyCycle";
    case DeviceManager::CommandType::RandomData:
        return "RandomData";
    case DeviceManager::CommandType::DataLog:
        return "DataLog";
    case DeviceManager::CommandType::Generic:
        return "Generic";
    }
    return "Unknown";
}

static const char *ledModeName(LedMode mode)
{
    switch (mode)
    {
    case LedMode::Booting:
        return "Booting";
    case LedMode::WaitingForStart:
        return "WaitingForStart";
    case LedMode::WifiConnecting:
        return "WifiConnecting";
    case LedMode::WifiConnected:
        return "WifiConnected";
    case LedMode::DeviceReady:
        return "DeviceReady";
    case LedMode::Error:
        return "Error";
    }
    return "Unknown";
}
