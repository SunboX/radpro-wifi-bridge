#pragma once

#include <Arduino.h>

namespace CooperativePump
{
using Callback = void (*)();

void setCallback(Callback callback);
void clearCallback();
void service();
} // namespace CooperativePump
