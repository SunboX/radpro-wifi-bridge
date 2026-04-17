#pragma once

#include <WiFiManager.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "Publishing/PublisherHealth.h"

class BridgeInfoPage
{
public:
    BridgeInfoPage(const PublisherHealth &openSenseMapHealth,
                   const PublisherHealth &gmcMapHealth,
                   const PublisherHealth &radmonHealth);

    void handlePage(WiFiManager *manager);
    void handleJson(WiFiManager *manager);

private:
    String collectJson() const;

    const PublisherHealth &openSenseMapHealth_;
    const PublisherHealth &gmcMapHealth_;
    const PublisherHealth &radmonHealth_;
};
