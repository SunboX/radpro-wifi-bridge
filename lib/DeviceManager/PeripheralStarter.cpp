#include "PeripheralStarter.h"

PeripheralStarter::PeripheralStarter(DeviceManager &deviceManager,
                                     UsbCdcHost &usbHost,
                                     MqttPublisher &mqtt,
                                     OpenSenseMapPublisher &osem,
                                     GmcMapPublisher &gmc,
                                     RadmonPublisher &radmon,
                                     LedController &led,
                                     DebugLogStream &log,
                                     bool allowEarlyStart,
                                     const char *firmwareVersion)
    : deviceManager_(deviceManager),
      usbHost_(usbHost),
      mqtt_(mqtt),
      osem_(osem),
      gmc_(gmc),
      radmon_(radmon),
      led_(led),
      log_(log),
      allowEarlyStart_(allowEarlyStart),
      firmwareVersion_(firmwareVersion)
{
}

void PeripheralStarter::startIfNeeded(bool wifiConnected, const std::vector<std::pair<uint16_t, uint16_t>> &vidPidAllowlist)
{
    if (started_ || !wifiConnected)
        return;

    const unsigned long now = millis();
    // Back off between USB host retries to avoid log spam.
    if (lastUsbRetryMs_ != 0 && now - lastUsbRetryMs_ < 3000)
        return;

    deviceManager_.begin(vidPidAllowlist);

    // Start USB host + CDC listener + TX task
    if (!usbHost_.begin())
    {
        esp_err_t err = usbHost_.lastError();
        const bool logNeeded = (err != lastUsbErr_) || lastUsbRetryMs_ == 0;
        lastUsbErr_ = err;

        // Treat missing host controller (ESP_ERR_NOT_FOUND) as transient; retry later without latching fault.
        const unsigned long backoffMs = (err == ESP_ERR_NOT_FOUND) ? 10000UL : 3000UL;
        lastUsbRetryMs_ = now + backoffMs;

        if (logNeeded)
        {
            log_.print("ERROR: usb.begin() failed");
            if (err != ESP_OK)
            {
                log_.print(" (");
                log_.print(esp_err_to_name(err));
                log_.print(")");
            }
            log_.print(" next retry in ");
            log_.print(backoffMs / 1000);
            log_.println("s");
        }

        if (err != ESP_ERR_NOT_FOUND)
        {
            led_.activateFault(FaultCode::UsbInterfaceFailure);
        }
        return;
    }
    else
    {
        log_.println("usb.begin() OK");
        led_.clearFault(FaultCode::UsbInterfaceFailure);
        lastUsbErr_ = ESP_OK;
        if (allowEarlyStart_)
        {
            log_.println("Send 'start', 'delay <ms>', or 'raw on/off/toggle' on this port.");
        }
    }

    mqtt_.begin();
    mqtt_.setBridgeVersion(firmwareVersion_);
    osem_.begin();
    gmc_.begin();
    radmon_.begin();
    started_ = true;
    lastUsbRetryMs_ = 0;
}
