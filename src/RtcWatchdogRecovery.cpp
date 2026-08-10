// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RtcWatchdogRecovery.h"

#include "esp_attr.h"
#include "esp_freertos_hooks.h"
#include "hal/wdt_hal.h"
#include "sdkconfig.h"
#include "soc/rtc_cntl_struct.h"

#ifndef CONFIG_BOOTLOADER_WDT_DISABLE_IN_USER_CODE
#error "RTC watchdog recovery requires CONFIG_BOOTLOADER_WDT_DISABLE_IN_USER_CODE"
#endif

namespace
{
wdt_hal_context_t rtcWatchdogContext{};
volatile uint32_t ticksSinceFeed = 0;
bool active = false;

void prepareContext()
{
    rtcWatchdogContext.inst = WDT_RWDT;
    rtcWatchdogContext.rwdt_dev = &RTCCNTL;
}

void IRAM_ATTR rtcWatchdogTickHook()
{
    if (++ticksSinceFeed < CONFIG_FREERTOS_HZ)
        return;

    ticksSinceFeed = 0;
    wdt_hal_write_protect_disable(&rtcWatchdogContext);
    wdt_hal_feed(&rtcWatchdogContext);
    wdt_hal_write_protect_enable(&rtcWatchdogContext);
}

void disableRtcWatchdog()
{
    wdt_hal_write_protect_disable(&rtcWatchdogContext);
    wdt_hal_disable(&rtcWatchdogContext);
    wdt_hal_write_protect_enable(&rtcWatchdogContext);
}
} // namespace

namespace RtcWatchdogRecovery
{
bool begin()
{
    prepareContext();
    ticksSinceFeed = 0;

    if (esp_register_freertos_tick_hook_for_cpu(rtcWatchdogTickHook, 0) != ESP_OK)
    {
        disableRtcWatchdog();
        active = false;
        return false;
    }

    active = true;
    return true;
}

bool isActive()
{
    return active;
}
} // namespace RtcWatchdogRecovery
