// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>

#include "RtcWatchdogRecovery.h"

// Forward declarations (Arduino.h usually declares these, but this is explicit)
void setup();
void loop();

extern "C" void app_main(void)
{
    // Initialize Arduino (pins, serial, timers, etc.)
    initArduino();

    // Keep the boot RTC watchdog as a stronger recovery path. The CPU0 tick
    // hook stops feeding it if the SYSTIMER/tick path stalls.
    RtcWatchdogRecovery::begin();

    // Run your Arduino sketch entry points
    setup();
    for (;;)
    {
        loop();
        // Yield so other FreeRTOS tasks (IDF + yours) can run
        delay(1); // or: vTaskDelay(1);
    }
}
