#include <Arduino.h>
#include "UsbCdcHost.h"
#include "esp_log.h"

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
static bool rawUsbLoggingEnabled = false;
static bool deviceIdRequestSent = false;
static bool deviceIdAwaitingResponse = false;
static bool deviceIdAnnouncePending = false;
static unsigned long deviceIdReadyAtMs = 0;
static unsigned long deviceIdLastRequestMs = 0;

// Forward declarations
static void handleStartupLogic();
static void runMainLogic();
static void sendDeviceIdCommand(bool announce);

// =========================
// USB Host wrapper
// =========================
static UsbCdcHost usb;

// Device connection callbacks
static void onUsbConnected()
{
    DBG.println("[main] USB device CONNECTED");
    deviceIdRequestSent = false;
    deviceIdAnnouncePending = true;
    deviceIdReadyAtMs = millis() + 200;
}

static void onUsbDisconnected()
{
    DBG.println("[main] USB device DISCONNECTED");
    deviceIdAwaitingResponse = false;
    deviceIdRequestSent = false;
    deviceIdAnnouncePending = true;
    deviceIdReadyAtMs = millis() + 200;
}

// Line-based RX callback (already line-buffered by UsbCdcHost)
static void onUsbLine(const String &line)
{
    DBG.print("[main] <- Line: ");
    DBG.println(line);

    if (line.startsWith("OK "))
    {
        int deviceIdIndex = line.lastIndexOf(';');
        if (deviceIdIndex >= 0 && deviceIdIndex + 1 < line.length())
        {
            String deviceId = line.substring(deviceIdIndex + 1);
            DBG.print("[main] Device ID: ");
            DBG.println(deviceId);
        }
    }

    deviceIdAwaitingResponse = false;
    deviceIdRequestSent = true;
    deviceIdAnnouncePending = false;
}

static void onUsbRaw(const uint8_t *data, size_t len)
{
    if (!rawUsbLoggingEnabled)
        return;
    DBG.print("[main] <- Raw: ");
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

static void sendDeviceIdCommand(bool announce)
{
    if (!usb.isConnected())
        return;

    if (announce)
        DBG.println("[main] -> Queue: GET deviceId");

    usb.sendCommand("GET deviceId", true);    // CRLF
    usb.sendCommand("GET deviceId\n", false); // LF
    usb.sendCommand("GET deviceId\r", false); // CR

    deviceIdRequestSent = true;
    deviceIdAwaitingResponse = true;
    deviceIdAnnouncePending = false;
    deviceIdLastRequestMs = millis();
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

    // Silence noisy CDC driver errors when using vendor-specific transport
    esp_log_level_set("cdc_acm_ops", ESP_LOG_NONE);
    // Keep UsbCdcHost chatter silent unless debugging
    esp_log_level_set("UsbCdcHost", ESP_LOG_NONE);
    // Suppress USB host stack info/error prints for cleaner output
    esp_log_level_set("USBH", ESP_LOG_NONE);

    // Record start time for non-blocking startup delay
    startupStartTime = millis();

    // Simple LED cue on boot (dim green)
    neopixelWrite(RGB_BUILTIN, 0, 8, 0);

    // Register callbacks
    usb.setDeviceCallbacks(onUsbConnected, onUsbDisconnected);
    usb.setLineCallback(onUsbLine);
    usb.setRawCallback(onUsbRaw);

    // BOSEAN FS-600 has CH340 chip (VID 0x1A86, PID 0x7523)
    usb.setVidPidFilter(0x1A86, 0x7523);

    // Optionally set target baud for CDC device
    // usb.setBaud(115200);

    // Start USB host + CDC listener + TX task
    if (!usb.begin())
    {
        DBG.println("[main] ERROR: usb.begin() failed");
    }
    else
    {
        DBG.println("[main] usb.begin() OK");
        if (ALLOW_EARLY_START)
        {
            DBG.println("Send 'start', 'delay <ms>', or 'raw on/off/toggle' on this port.");
        }

        deviceIdRequestSent = false;
        deviceIdAwaitingResponse = false;
        deviceIdAnnouncePending = true;
        deviceIdReadyAtMs = millis();
    }
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

    const unsigned long now = millis();

    if (usb.isConnected())
    {
        if (deviceIdAwaitingResponse)
        {
            constexpr unsigned long DEVICE_ID_TIMEOUT_MS = 2000;
            if (now - deviceIdLastRequestMs > DEVICE_ID_TIMEOUT_MS)
            {
                deviceIdAwaitingResponse = false;
                deviceIdRequestSent = false;
                deviceIdAnnouncePending = true;
                deviceIdReadyAtMs = now + 200;
            }
        }
        else if (!deviceIdRequestSent && (long)(now - deviceIdReadyAtMs) >= 0)
        {
            sendDeviceIdCommand(deviceIdAnnouncePending);
        }
    }

    // Keep loop snappy; USB host runs in its own tasks
    delay(5);
}

// =========================
// Startup state machine
// =========================
static void handleStartupLogic()
{
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
            rawUsbLoggingEnabled = true;
            DBG.println("USB raw logging enabled.");
        }
        else if (command.equalsIgnoreCase("raw off"))
        {
            rawUsbLoggingEnabled = false;
            DBG.println("USB raw logging disabled.");
        }
        else if (command.equalsIgnoreCase("raw toggle"))
        {
            rawUsbLoggingEnabled = !rawUsbLoggingEnabled;
            DBG.print("USB raw logging toggled ");
            DBG.println(rawUsbLoggingEnabled ? "ON." : "OFF.");
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
        // brief green pulse
        static uint8_t ticks = 0;
        if (ticks < 6)
        { // ~30ms total at 5ms loop delay
            neopixelWrite(RGB_BUILTIN, 0, 32, 0);
            ticks++;
        }
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
    // Task 1: RGB LED heartbeat every 250 ms (dim green)
    static unsigned long lastLedTime = 0;
    if (millis() - lastLedTime >= 250)
    {
        lastLedTime = millis();
        const uint8_t r = 0, g = 10, b = 0;
        neopixelWrite(RGB_BUILTIN, r, g, b);
    }

    // Task 2: Log once per second on debug UART
    static unsigned long lastLoopTime = 0;
    if (millis() - lastLoopTime >= 1000)
    {
        lastLoopTime = millis();
        DBG.println("Main loop is running.");
    }
}
