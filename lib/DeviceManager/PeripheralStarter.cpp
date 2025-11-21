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

    deviceManager_.begin(vidPidAllowlist);

    // Start USB host + CDC listener + TX task
    if (!usbHost_.begin())
    {
        log_.println("ERROR: usb.begin() failed");
        led_.activateFault(FaultCode::UsbInterfaceFailure);
    }
    else
    {
        log_.println("usb.begin() OK");
        led_.clearFault(FaultCode::UsbInterfaceFailure);
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
}
