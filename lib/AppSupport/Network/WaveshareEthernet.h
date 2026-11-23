/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <Arduino.h>

namespace WaveshareEthernet
{
bool begin(const char *hostname);
bool started();
bool linkUp();
bool connected();
IPAddress localIP();
IPAddress gatewayIP();
IPAddress subnetMask();
String macAddress();
} // namespace WaveshareEthernet
