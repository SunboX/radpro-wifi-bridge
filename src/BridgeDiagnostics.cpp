#include "BridgeDiagnostics.h"
#include <WiFi.h>

#ifndef USB_DEBUG_LOGS_ENABLED
#define USB_DEBUG_LOGS_ENABLED 1
#endif

BridgeDiagnostics::BridgeDiagnostics(Print &log, LedController &led)
    : log_(log), led_(led), usb_debug_enabled_(USB_DEBUG_LOGS_ENABLED != 0)
{
}

void BridgeDiagnostics::initialize()
{
    applyUsbLogLevels(false);
}

void BridgeDiagnostics::handleLine(const String &line)
{
    log_.println(line);

    if (line == "USB device CONNECTED")
    {
        led_.clearFault(FaultCode::UsbDeviceGone);
    }
    else if (line == "USB device DISCONNECTED")
    {
        led_.activateFault(FaultCode::UsbDeviceGone);
    }
    else if (line.startsWith("Device ID:"))
    {
        led_.clearFault(FaultCode::DeviceIdTimeout);
    }
    else if (line.startsWith("Tube Sensitivity:"))
    {
        led_.clearFault(FaultCode::MissingSensitivity);
    }
}

void BridgeDiagnostics::handleRaw(const uint8_t *data, size_t len)
{
    if (!data || !len)
        return;

    log_.print("<- Raw: ");
    for (size_t i = 0; i < len; ++i)
    {
        if (i)
            log_.print(' ');
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X", data[i]);
        log_.print(buf);
    }
    log_.println();
}

void BridgeDiagnostics::setUsbDebugEnabled(bool enabled, bool announce)
{
    if (usb_debug_enabled_ == enabled)
        return;

    usb_debug_enabled_ = enabled;
    applyUsbLogLevels(announce);
}

void BridgeDiagnostics::applyUsbLogLevels(bool announce)
{
    if (usb_debug_enabled_)
    {
        esp_log_level_set("cdc_acm_ops", ESP_LOG_INFO);
        esp_log_level_set("UsbCdcHost", ESP_LOG_INFO);
        esp_log_level_set("USBH", ESP_LOG_INFO);
        if (announce)
            log_.println("USB debug logging ENABLED (cdc_acm_ops/UsbCdcHost/USBH=INFO).");
    }
    else
    {
        esp_log_level_set("cdc_acm_ops", ESP_LOG_NONE);
        esp_log_level_set("UsbCdcHost", ESP_LOG_WARN);
        esp_log_level_set("USBH", ESP_LOG_NONE);
        if (announce)
            log_.println("USB debug logging disabled (restored quiet log levels).");
    }
}

void BridgeDiagnostics::updateLedStatus(bool running, bool deviceError, bool mqttError, bool deviceReady)
{
    if (!running)
    {
        led_.setMode(LedMode::WaitingForStart);
        return;
    }

    if (deviceError || mqttError)
    {
        led_.setMode(LedMode::Error);
        return;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        led_.setMode(LedMode::WifiConnecting);
        return;
    }

    if (!deviceReady)
    {
        led_.setMode(LedMode::WifiConnected);
        return;
    }

    led_.setMode(LedMode::DeviceReady);
}
