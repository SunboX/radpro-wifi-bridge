#include <Arduino.h>
extern "C"
{
#include "usb/usb_host.h"
#include "usb/usb_types_stack.h" // usb_device_desc_t
}
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

// Forward declarations
static void handleStartupLogic();
static void runMainLogic();

// =========================
// USB Host plumbing
// =========================
static const char *TAG = "USBHOST";

static usb_host_client_handle_t clientH = nullptr;
static TaskHandle_t libTaskH = nullptr;
static TaskHandle_t clientTaskH = nullptr;

static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    switch (event_msg->event)
    {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
    {
        const uint8_t addr = event_msg->new_dev.address;
#if defined(ESP_IDF_VERSION_MAJOR) && defined(ESP_IDF_VERSION_MINOR) && \
    ((ESP_IDF_VERSION_MAJOR > 4) || (ESP_IDF_VERSION_MAJOR == 4 && ESP_IDF_VERSION_MINOR >= 99))
        const usb_speed_t spd = event_msg->new_dev.speed;
        ESP_LOGI(TAG, "NEW_DEV: addr=%u speed=%s",
                 addr, spd == USB_SPEED_LOW ? "LOW" : spd == USB_SPEED_FULL ? "FULL"
                                                                            : "HIGH?");
#else
        ESP_LOGI(TAG, "NEW_DEV: addr=%u (speed n/a on this IDF)", addr);
#endif

        usb_device_handle_t devH = nullptr;
        if (usb_host_device_open(clientH, addr, &devH) == ESP_OK)
        {
            const usb_device_desc_t *desc = nullptr;
            if (usb_host_get_device_descriptor(devH, &desc) == ESP_OK && desc)
            {
                ESP_LOGI(TAG, "Device: VID=0x%04X PID=0x%04X bDeviceClass=0x%02X bNumCfg=%u",
                         desc->idVendor, desc->idProduct, desc->bDeviceClass, desc->bNumConfigurations);
                ESP_LOGI(TAG, "USB version %x.%02x, MaxPacket0=%u",
                         desc->bcdUSB >> 8, desc->bcdUSB & 0xFF, desc->bMaxPacketSize0);
            }
            else
            {
                ESP_LOGW(TAG, "Failed to get device descriptor");
            }
            usb_host_device_close(clientH, devH);
        }
        else
        {
            ESP_LOGW(TAG, "usb_host_device_open failed");
        }
        break;
    }

    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        ESP_LOGI(TAG, "Device disconnected");
        break;

    default:
        ESP_LOGD(TAG, "Client event: %d", (int)event_msg->event);
        break;
    }
}

static void usb_lib_task(void *arg)
{
    ESP_LOGI(TAG, "Library task started");
    while (true)
    {
        uint32_t flags = 0;
        esp_err_t err = usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "usb_host_lib_handle_events err=0x%x", err);
        }
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            ESP_LOGW(TAG, "NO_CLIENTS -> usb_host_device_free_all()");
            usb_host_device_free_all();
        }
        if (flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
        {
            ESP_LOGI(TAG, "ALL_FREE (nothing attached)");
        }
    }
}

static void usb_client_task(void *arg)
{
    ESP_LOGI(TAG, "Client task started");
    while (true)
    {
        // Pump messages to client_event_cb (REQUIRED)
        esp_err_t err = usb_host_client_handle_events(clientH, portMAX_DELAY);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "usb_host_client_handle_events err=0x%x", err);
        }
    }
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

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);

    Serial.println("\nESP32-S3 USB Host (enumerate VID/PID)");
    ESP_LOGI(TAG, "Booting… free heap=%u", (unsigned)ESP.getFreeHeap());

    // Record the start time for non-blocking startup delay
    startupStartTime = millis();

    // Simple LED cue on boot (dim green)
    neopixelWrite(RGB_BUILTIN, 0, 8, 0);

    // Host install
    usb_host_config_t host_config = {}; // default: task runs in caller context
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    ESP_LOGI(TAG, "USB Host installed");

    // Start library event task
    xTaskCreatePinnedToCore(usb_lib_task, "usb_lib", 4096, nullptr, 20, &libTaskH, APP_CPU_NUM);

    // Register async client
    usb_host_client_config_t cfg = {
        .is_synchronous = false,
        .max_num_event_msg = 16,
        .async = {.client_event_callback = client_event_cb, .callback_arg = nullptr}};
    ESP_ERROR_CHECK(usb_host_client_register(&cfg, &clientH));
    ESP_LOGI(TAG, "USB Host client registered");

    // Start client event pump task
    xTaskCreatePinnedToCore(usb_client_task, "usb_client", 4096, nullptr, 21, &clientTaskH, APP_CPU_NUM);

    // User guidance on the debug UART (second USB port)
    DBG.println("Initializing RadPro WiFi Bridge…");
    if (ALLOW_EARLY_START)
    {
        DBG.println("Send 'start' or 'delay <ms>' on this port to begin early / change delay.");
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
        // Place additional non-blocking work here
    }
}
