# RTC Watchdog Recovery Diagnostic Design

## Context

Issue #21 reproduced on `1.15.14-test.1` after the secondary USB descriptor client was disabled. The exact ELF decodes the watchdog reset PCs into the FreeRTOS tick/watchdog path. During the failed reboot, CPU0 was stuck in `systimer_hal_get_counter_value()` waiting indefinitely for the SYSTIMER snapshot-valid bit while CPU1 was idle.

The existing Timer Group watchdogs reset the main digital system but not the RTC domain. That reset did not clear the observed failure during the next boot. This diagnostic must improve recovery without claiming that software, the power supply, or the board has been proven as the trigger.

## Considered Approaches

1. **Feed the retained RTC watchdog from the main loop.** This directly proves main-loop progress, but it would reset healthy devices while the initial Wi-Fi configuration portal is blocking inside `setup()`.
2. **Feed the retained RTC watchdog from the CPU0 FreeRTOS tick hook.** This remains active during normal blocking setup and onboarding, but stops when the CPU0 tick/interrupt path stalls as shown in the reporter's log. This is the selected approach.
3. **Use an external watchdog or higher brownout threshold.** These remain valuable later A/B tests, but combining them with this firmware would change more than one variable.

## Design

- Base `1.15.14-test.2` on the exact `1.15.14-test.1` diagnostic branch so the descriptor client remains disabled.
- Keep the bootloader RTC watchdog enabled in user code with a 30-second timeout and its existing `RESET_RTC` action.
- Register one IRAM-safe CPU0 FreeRTOS tick hook after Arduino initialization.
- Feed the RTC watchdog once per second from that hook, not on every 1 kHz tick.
- If hook registration fails, disable the retained RTC watchdog to avoid a boot loop and report that recovery is unavailable.
- Print one explicit startup line identifying whether RTC recovery is active.
- Do not alter the brownout threshold, USB behavior, Wi-Fi behavior, task priorities, Timer Group watchdog settings, or stable `1.15.13` release.

## Expected Result and Limits

If CPU0's SYSTIMER/tick path stalls again, the existing interrupt/task watchdog can still perform its first system reset. If the following boot also stalls, the retained RTC watchdog should expire and perform the stronger RTC-domain reset. The expected reset reason is `RTCWDT_RTC_RST`.

This is a recovery workaround and diagnostic build, not a confirmed root-cause fix. It cannot correct a persistent power or board fault, and it still requires physical testing on the reporter's bridge and a known genuine ESP32-S3-DevKitC-1.
