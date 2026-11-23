/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <WiFi.h>
#include "Network/WaveshareEthernet.h"

namespace NetworkStatus
{
inline bool ethernetStarted()
{
    return WaveshareEthernet::started();
}

inline bool ethernetLinkUp()
{
    return WaveshareEthernet::linkUp();
}

inline bool ethernetConnected()
{
    return WaveshareEthernet::connected();
}

inline bool wifiConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

inline bool connected()
{
    return ethernetConnected() || wifiConnected();
}

inline bool usingEthernet()
{
    return ethernetConnected();
}

inline IPAddress localIP()
{
    if (ethernetConnected())
        return WaveshareEthernet::localIP();
    return WiFi.localIP();
}

inline IPAddress gatewayIP()
{
    if (ethernetConnected())
        return WaveshareEthernet::gatewayIP();
    return WiFi.gatewayIP();
}

inline IPAddress subnetMask()
{
    if (ethernetConnected())
        return WaveshareEthernet::subnetMask();
    return WiFi.subnetMask();
}

inline String macAddress()
{
    if (ethernetConnected())
        return WaveshareEthernet::macAddress();
    return WiFi.macAddress();
}
} // namespace NetworkStatus
