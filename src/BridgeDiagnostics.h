#pragma once

#include <Arduino.h>
#include "Led/LedController.h"
#include "esp_log.h"

class BridgeDiagnostics
{
public:
    BridgeDiagnostics(Print &log, LedController &led);

    void initialize();

    void handleLine(const String &line);
    void handleRaw(const uint8_t *data, size_t len);

    void setUsbDebugEnabled(bool enabled, bool announce = true);
    void toggleUsbDebug() { setUsbDebugEnabled(!usb_debug_enabled_, true); }
    bool usbDebugEnabled() const { return usb_debug_enabled_; }

    void updateLedStatus(bool running, bool deviceError, bool mqttError, bool deviceReady);

private:
    void applyUsbLogLevels(bool announce);

    Print &log_;
    LedController &led_;
    bool usb_debug_enabled_;
};
