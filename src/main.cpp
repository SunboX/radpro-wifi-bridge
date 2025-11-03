#include <Arduino.h>
#include "UsbCdcHost.h"
#include "esp_log.h"
#include "esp_system.h"
#include <cstring>
#include <WiFi.h>
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

#ifndef USB_DEBUG_LOGS_ENABLED
#define USB_DEBUG_LOGS_ENABLED 1
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
#define INITIAL_STARTUP_DELAY_MS 5000
#define ALLOW_EARLY_START true

// Program state
static bool isRunning = false;
static unsigned long startupDelayMs = INITIAL_STARTUP_DELAY_MS;
static unsigned long startupStartTime = 0;
// Forward declarations
static void handleStartupLogic();
static void runMainLogic();
static void logLine(const String &msg);
static void logRaw(const uint8_t *data, size_t len);
static void updateLedStatus();
static void configureUsbLogging(bool announce);

// =========================
// USB Host wrapper
// =========================
static UsbCdcHost usb;
static DeviceManager device_manager(usb);

static LedController ledController;
static void logLine(const String &msg)
{
    DBG.println(msg);
    if (msg == "USB device CONNECTED")
    {
        ledController.clearFault(FaultCode::UsbDeviceGone);
    }
    else if (msg == "USB device DISCONNECTED")
    {
        ledController.activateFault(FaultCode::UsbDeviceGone);
    }
    else if (msg.startsWith("Device ID:"))
    {
        ledController.clearFault(FaultCode::DeviceIdTimeout);
    }
    else if (msg.startsWith("Tube Sensitivity:"))
    {
        ledController.clearFault(FaultCode::MissingSensitivity);
    }
}

static void logRaw(const uint8_t *data, size_t len)
{
    DBG.print("<- Raw: ");
    for (size_t i = 0; i < len; ++i)
    {
        if (i)
            DBG.print(' ');
        char buf[4];
        sprintf(buf, "%02X", data[i]);
        DBG.print(buf);
    }
    DBG.println();
}

static AppConfig appConfig;
static AppConfigStore configStore;
static WiFiPortalService portalService(appConfig, configStore, DBG, ledController);
static MqttPublisher mqttPublisher(appConfig, DBG, ledController);
static OpenSenseMapPublisher openSenseMapPublisher(appConfig, DBG, BRIDGE_FIRMWARE_VERSION);
static GmcMapPublisher gmcMapPublisher(appConfig, DBG, BRIDGE_FIRMWARE_VERSION);
static RadmonPublisher radmonPublisher(appConfig, DBG, BRIDGE_FIRMWARE_VERSION);
static bool deviceReady = false;
static bool deviceError = false;
static bool mqttError = false;
static bool usbDebugLogsEnabled = (USB_DEBUG_LOGS_ENABLED != 0);

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

    configureUsbLogging(true);

    // Record start time for non-blocking startup delay
    startupStartTime = millis();

    device_manager.setLineHandler(logLine);
    device_manager.setRawHandler(logRaw);
    device_manager.setCommandResultHandler([&](DeviceManager::CommandType type, const String &value, bool success)
                                           {
        if (!success)
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

        mqttPublisher.onCommandResult(type, value);
        openSenseMapPublisher.onCommandResult(type, value);
        gmcMapPublisher.onCommandResult(type, value);
        radmonPublisher.onCommandResult(type, value); });
    device_manager.begin(0x1A86, 0x7523);

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
    updateLedStatus();
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
    updateLedStatus();
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
        else if (command.equalsIgnoreCase("usb debug on"))
        {
            usbDebugLogsEnabled = true;
            configureUsbLogging(true);
        }
        else if (command.equalsIgnoreCase("usb debug off"))
        {
            usbDebugLogsEnabled = false;
            configureUsbLogging(true);
        }
        else if (command.equalsIgnoreCase("usb debug toggle"))
        {
            usbDebugLogsEnabled = !usbDebugLogsEnabled;
            configureUsbLogging(true);
        }
        else if (command.equalsIgnoreCase("raw toggle"))
        {
            device_manager.toggleRawLogging();
            DBG.print("USB raw logging toggled ");
            DBG.println(device_manager.rawLoggingEnabled() ? "ON." : "OFF.");
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

static void updateLedStatus()
{
    if (!isRunning)
    {
        ledController.setMode(LedMode::WaitingForStart);
        return;
    }

    if (deviceError || mqttError)
    {
        ledController.setMode(LedMode::Error);
        return;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        ledController.setMode(LedMode::WifiConnecting);
        return;
    }

    if (!deviceReady)
    {
        ledController.setMode(LedMode::WifiConnected);
        return;
    }

    ledController.setMode(LedMode::DeviceReady);
}

static void configureUsbLogging(bool announce)
{
    if (usbDebugLogsEnabled)
    {
        esp_log_level_set("cdc_acm_ops", ESP_LOG_INFO);
        esp_log_level_set("UsbCdcHost", ESP_LOG_INFO);
        esp_log_level_set("USBH", ESP_LOG_INFO);
        if (announce)
            DBG.println("USB debug logging ENABLED (cdc_acm_ops/UsbCdcHost/USBH=INFO).");
    }
    else
    {
        esp_log_level_set("cdc_acm_ops", ESP_LOG_NONE);
        esp_log_level_set("UsbCdcHost", ESP_LOG_WARN);
        esp_log_level_set("USBH", ESP_LOG_NONE);
        if (announce)
            DBG.println("USB debug logging disabled (restored quiet log levels).");
    }
}
