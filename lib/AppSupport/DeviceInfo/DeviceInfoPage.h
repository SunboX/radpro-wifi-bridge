/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <WiFiManager.h>
#include <WebServer.h>
#include "DeviceInfoStore.h"

class DeviceInfoPage
{
public:
    explicit DeviceInfoPage(DeviceInfoStore &store);

    void handlePage(WiFiManager *manager);
    void handleJson(WiFiManager *manager);
    void handlePage(WebServer *server);
    void handleJson(WebServer *server);
    void handlePrometheus(WiFiManager *manager);
    void handlePrometheus(WebServer *server);

private:
    DeviceInfoStore &store_;
};
