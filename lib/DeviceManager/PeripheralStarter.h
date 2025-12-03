#pragma once

#include <vector>
#include <utility>
#include "DeviceManager.h"
#include "UsbCdcHost.h"
#include "Led/LedController.h"
#include "Logging/DebugLogStream.h"
#include "Mqtt/MqttPublisher.h"
#include "OpenSenseMap/OpenSenseMapPublisher.h"
#include "GmcMap/GmcMapPublisher.h"
#include "Radmon/RadmonPublisher.h"

class PeripheralStarter
{
public:
    PeripheralStarter(DeviceManager &deviceManager,
                      UsbCdcHost &usbHost,
                      MqttPublisher &mqtt,
                      OpenSenseMapPublisher &osem,
                      GmcMapPublisher &gmc,
                      RadmonPublisher &radmon,
                      LedController &led,
                      DebugLogStream &log,
                      bool allowEarlyStart,
                      const char *firmwareVersion);

    void startIfNeeded(bool wifiConnected, bool timeSynced, const std::vector<std::pair<uint16_t, uint16_t>> &vidPidAllowlist);
    bool started() const { return started_; }

private:
    DeviceManager &deviceManager_;
    UsbCdcHost &usbHost_;
    MqttPublisher &mqtt_;
    OpenSenseMapPublisher &osem_;
    GmcMapPublisher &gmc_;
    RadmonPublisher &radmon_;
    LedController &led_;
    DebugLogStream &log_;
    bool allowEarlyStart_;
    const char *firmwareVersion_;
    bool started_ = false;
    unsigned long lastUsbRetryMs_ = 0;
    unsigned long nextUsbRetryAtMs_ = 0;
    esp_err_t lastUsbErr_ = ESP_OK;
};
