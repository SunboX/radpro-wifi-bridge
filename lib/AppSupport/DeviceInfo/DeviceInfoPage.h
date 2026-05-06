/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

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
    DeviceInfoStore &store_;
};
