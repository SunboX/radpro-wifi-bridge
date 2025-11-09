#pragma once

#include <WiFiManager.h>
#include "DeviceInfoStore.h"

class DeviceInfoPage
{
public:
    explicit DeviceInfoPage(DeviceInfoStore &store);

    void handlePage(WiFiManager *manager);
    void handleJson(WiFiManager *manager);

private:
    String buildHtml() const;
    String buildScript() const;

    DeviceInfoStore &store_;
};
