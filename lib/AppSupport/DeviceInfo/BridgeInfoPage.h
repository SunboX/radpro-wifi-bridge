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
                   const PublisherHealth &radmonHealth,
                   const PublisherHealth &openRadiationHealth,
                   const PublisherHealth &safecastHealth);

    void handlePage(WiFiManager *manager);
    void handleJson(WiFiManager *manager);

private:
    String collectJson() const;

    const PublisherHealth &openSenseMapHealth_;
    const PublisherHealth &gmcMapHealth_;
    const PublisherHealth &radmonHealth_;
    const PublisherHealth &openRadiationHealth_;
    const PublisherHealth &safecastHealth_;
};
