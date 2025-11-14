#include "DeviceInfoPage.h"
#include <LittleFS.h>

DeviceInfoPage::DeviceInfoPage(DeviceInfoStore &store) : store_(store) {}

void DeviceInfoPage::handlePage(WiFiManager *manager)
{
    if (!manager || !manager->server)
        return;

    File file = LittleFS.open("/portal/device-info.html", "r");
    if (!file)
    {
        manager->server->send(500, "text/plain", "Device info page missing.");
        return;
    }

    String html = file.readString();
    file.close();
    manager->server->send(200, "text/html", html);
}

void DeviceInfoPage::handleJson(WiFiManager *manager)
{
    if (!manager || !manager->server)
        return;
    manager->server->send(200, "application/json", store_.toJson());
}
