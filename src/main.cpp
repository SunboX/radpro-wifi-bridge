#include <Arduino.h>
#include "UsbCdcHost.h"
#include "esp_log.h"
#include <cstring>
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "DeviceManager.h"
#include "AppConfig/AppConfig.h"
#include "ConfigPortal/WiFiPortalService.h"
#include "Led/LedController.h"
#include "Mqtt/MqttPublisher.h"

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

// =========================
// USB Host wrapper
// =========================
static UsbCdcHost usb;
static DeviceManager device_manager(usb);
static void logLine(const String &msg)
{
    DBG.println(msg);
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
static WiFiPortalService portalService(appConfig, configStore, DBG);
static MqttPublisher mqttPublisher(appConfig, DBG);
static LedController ledController;
static bool deviceReady = false;
static bool deviceError = false;
static bool mqttError = false;

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

    ledController.begin();
    ledController.setMode(LedMode::Booting);
    ledController.update();

    // Silence noisy CDC driver errors when using vendor-specific transport
    esp_log_level_set("cdc_acm_ops", ESP_LOG_NONE);
    // Keep UsbCdcHost chatter to warnings (use INFO for detailed debugging)
    esp_log_level_set("UsbCdcHost", ESP_LOG_WARN);
    // Suppress USB host stack info/error prints for cleaner output
    esp_log_level_set("USBH", ESP_LOG_NONE);

    // Record start time for non-blocking startup delay
    startupStartTime = millis();

    device_manager.setLineHandler(logLine);
    device_manager.setRawHandler(logRaw);
    device_manager.setCommandResultHandler([](DeviceManager::CommandType type, const String &value, bool success) {
        if (!success)
        {
            deviceError = true;
            if (type == DeviceManager::CommandType::DeviceId)
                deviceReady = false;
            DBG.print("Device command failed: ");
            DBG.println(static_cast<int>(type));
            ledController.triggerPulse(LedPulse::MqttFailure, 250);
            return;
        }

        deviceError = false;
        if (type == DeviceManager::CommandType::DeviceId)
            deviceReady = value.length() > 0;

        mqttPublisher.onCommandResult(type, value);
    });
    device_manager.begin(0x1A86, 0x7523);

    // Optionally set target baud for CDC device
    // usb.setBaud(115200);

    // Start USB host + CDC listener + TX task
    if (!usb.begin())
    {
        DBG.println("ERROR: usb.begin() failed");
    }
    else
    {
        DBG.println("usb.begin() OK");
        if (ALLOW_EARLY_START)
        {
            DBG.println("Send 'start', 'delay <ms>', or 'raw on/off/toggle' on this port.");
        }
    }

    if (!configStore.load(appConfig))
    {
        DBG.println("Preferences read failed; keeping defaults.");
    }

    portalService.begin();
    mqttPublisher.begin();
    mqttPublisher.setPublishCallback([](bool success) {
        if (success)
        {
            mqttError = false;
            ledController.triggerPulse(LedPulse::MqttSuccess, 150);
        }
        else
        {
            mqttError = true;
            DBG.println("MQTT publish failed.");
            ledController.triggerPulse(LedPulse::MqttFailure, 250);
        }
    });

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(appConfig.deviceName.c_str());

    if (!portalService.connect(false))
    {
        DBG.println("Auto-connect or portal timed out; starting configuration portal.");
        portalService.connect(true);
    }
    mqttPublisher.updateConfig();
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
