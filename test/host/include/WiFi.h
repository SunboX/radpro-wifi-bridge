/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

enum wl_status_t
{
    WL_IDLE_STATUS = 0,
    WL_CONNECTED = 3,
    WL_DISCONNECTED = 6
};

class WiFiClass
{
public:
    wl_status_t status() const
    {
        return status_;
    }

    void setStatus(wl_status_t status)
    {
        status_ = status;
    }

private:
    wl_status_t status_ = WL_DISCONNECTED;
};

inline WiFiClass WiFi;
