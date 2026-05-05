/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <Arduino.h>

namespace CooperativePump
{
using Callback = void (*)();

void setCallback(Callback callback);
void clearCallback();
void service();
} // namespace CooperativePump
