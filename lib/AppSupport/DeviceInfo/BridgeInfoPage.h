#pragma once

#include <WiFiManager.h>
#include <WiFi.h>
#include <esp_wifi.h>

class BridgeInfoPage
{
public:
    BridgeInfoPage();

    void handlePage(WiFiManager *manager);
    void handleJson(WiFiManager *manager);

private:
    String collectJson() const;
};
