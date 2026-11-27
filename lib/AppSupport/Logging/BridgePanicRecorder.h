// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <Arduino.h>
namespace BridgePanicRecorder {
void begin(Print &log);
uint32_t currentBootId();
String json();
}
