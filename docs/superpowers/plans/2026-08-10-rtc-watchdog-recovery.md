# RTC Watchdog Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish `1.15.14-test.2`, which retains the independent RTC watchdog and feeds it from CPU0 tick progress so a repeated SYSTIMER stall can receive a full RTC-domain reset.

**Architecture:** A small `RtcWatchdogRecovery` module owns the ESP-IDF watchdog HAL context and CPU0 tick hook. The bootloader configuration leaves the RTC watchdog enabled with a 30-second timeout; the hook feeds it once per second, and startup emits a diagnostic status line.

**Tech Stack:** C++17, ESP-IDF 4.4.7 watchdog HAL and FreeRTOS hooks, PlatformIO, Python host configuration test, GitHub CLI.

## Global Constraints

- Keep stable `1.15.13` and `main` unchanged.
- Base the prerelease on `1.15.14-test.1`; keep the USB descriptor client disabled.
- Change only RTC-watchdog recovery and version metadata.
- Do not change brownout, Wi-Fi, USB CDC, Timer Group watchdog, or task-priority behavior.
- Describe the result as an unconfirmed recovery workaround that still requires physical and genuine-board testing.

---

### Task 1: Add the RTC recovery contract

**Files:**
- Create: `test/host/rtc_watchdog_recovery_config_test.py`
- Create: `src/RtcWatchdogRecovery.h`
- Create: `src/RtcWatchdogRecovery.cpp`
- Modify: `src/app_main.cpp`
- Modify: `src/main.cpp`
- Modify: `sdkconfig.defaults`

**Interfaces:**
- Produces: `RtcWatchdogRecovery::begin() -> bool` and `RtcWatchdogRecovery::isActive() -> bool`.
- Consumes: ESP-IDF `esp_register_freertos_tick_hook_for_cpu()` and RTC watchdog HAL calls.

- [ ] **Step 1: Write and run the host configuration test; verify it fails because RTC recovery is absent.**
- [ ] **Step 2: Add the 30-second retained boot RTC-watchdog configuration.**
- [ ] **Step 3: Implement the CPU0 tick hook, one-second feed divider, and safe registration-failure fallback.**
- [ ] **Step 4: Initialize recovery before `setup()` and print its startup state.**
- [ ] **Step 5: Run the new test and the five existing diagnostic/recovery tests.**

### Task 2: Build and package `1.15.14-test.2`

**Files:**
- Modify: `platformio.ini`
- Modify: `version.txt`
- Generated: `.pio/build/esp32-s3-devkitc-1/*`
- Generated: `firmware_1.15.14-test.2.zip`

- [ ] **Step 1: Update both firmware version sources to `1.15.14-test.2`.**
- [ ] **Step 2: Build `buildprog` and `buildfs`.**
- [ ] **Step 3: Confirm the generated sdkconfig enables the retained 30-second RTC watchdog.**
- [ ] **Step 4: Regenerate the final five-entry ZIP and compare every extracted binary byte-for-byte with the final build outputs.**
- [ ] **Step 5: Record SHA-256 hashes and inspect the ELF symbols/configuration.**

### Task 3: Review and publish

**Files:**
- Commit all source, test, plan, and version changes; do not commit generated installer files or caches.

- [ ] **Step 1: Review the complete diff and run fresh tests/build verification.**
- [ ] **Step 2: Commit and push `diagnostic/issue-21-rtc-watchdog`.**
- [ ] **Step 3: Create and push tag `1.15.14-test.2`.**
- [ ] **Step 4: Publish a GitHub prerelease with the verified ZIP and explicit diagnostic limitations.**
- [ ] **Step 5: Post a detailed issue #21 comment in André's established style with the direct download and conclusion.**
- [ ] **Step 6: Verify the public release asset, prerelease state, tag target, comment, and that `1.15.13` remains the stable release.**
