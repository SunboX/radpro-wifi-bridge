#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 André Fiedler
#
# SPDX-License-Identifier: GPL-3.0-or-later

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def require(text: str, needle: str, source: str) -> None:
    assert needle in text, f"{source} is missing {needle!r}"


def main() -> None:
    sdkconfig = (ROOT / "sdkconfig.defaults").read_text()
    require(sdkconfig, "CONFIG_BOOTLOADER_WDT_ENABLE=y", "sdkconfig.defaults")
    require(
        sdkconfig,
        "CONFIG_BOOTLOADER_WDT_DISABLE_IN_USER_CODE=y",
        "sdkconfig.defaults",
    )
    require(sdkconfig, "CONFIG_BOOTLOADER_WDT_TIME_MS=30000", "sdkconfig.defaults")

    target_sdkconfig = (ROOT / "sdkconfig.esp32-s3-devkitc-1").read_text()
    require(
        target_sdkconfig,
        "CONFIG_BOOTLOADER_WDT_DISABLE_IN_USER_CODE=y",
        "sdkconfig.esp32-s3-devkitc-1",
    )
    require(
        target_sdkconfig,
        "CONFIG_BOOTLOADER_WDT_TIME_MS=30000",
        "sdkconfig.esp32-s3-devkitc-1",
    )

    app_main = (ROOT / "src/app_main.cpp").read_text()
    require(app_main, '#include "RtcWatchdogRecovery.h"', "src/app_main.cpp")
    require(
        app_main,
        "RtcWatchdogRecovery::begin();",
        "src/app_main.cpp",
    )
    assert app_main.index("RtcWatchdogRecovery::begin();") < app_main.rindex("setup();")

    recovery = (ROOT / "src/RtcWatchdogRecovery.cpp").read_text()
    require(
        recovery,
        "esp_register_freertos_tick_hook_for_cpu(rtcWatchdogTickHook, 0)",
        "src/RtcWatchdogRecovery.cpp",
    )
    require(recovery, "CONFIG_FREERTOS_HZ", "src/RtcWatchdogRecovery.cpp")
    require(recovery, "wdt_hal_feed", "src/RtcWatchdogRecovery.cpp")
    require(recovery, "wdt_hal_disable", "src/RtcWatchdogRecovery.cpp")

    main_cpp = (ROOT / "src/main.cpp").read_text()
    require(
        main_cpp,
        "RTC watchdog recovery active (30000 ms, fed from CPU0 tick)",
        "src/main.cpp",
    )

    print("RTC watchdog recovery configuration test passed")


if __name__ == "__main__":
    main()
