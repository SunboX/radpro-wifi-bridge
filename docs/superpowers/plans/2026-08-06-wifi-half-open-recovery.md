# Wi-Fi Half-Open Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the reconnect/DHCP race and recover from a stale `WL_CONNECTED` state by releasing firmware 1.15.13 with a conservative gateway-reachability monitor.

**Architecture:** Keep timing and health decisions in the existing host-testable reconnect policy. Make `WiFiPortalService` the sole owner of station reconnection, protect the DHCP interval after association, and use ESP-IDF's asynchronous ping API to prove and monitor gateway reachability without coupling Wi-Fi recovery to MQTT.

**Tech Stack:** C++17, Arduino-ESP32 2.0.17, ESP-IDF 4.4.7, PlatformIO, ESP-IDF `esp_ping`, host-side C++ tests, GitHub CLI.

---

## File Structure

- Modify: `lib/AppSupport/ConfigPortal/WiFiReconnectPolicy.h`
  - Add pure DHCP-in-flight, disconnect retry, and gateway health policy helpers.
- Modify: `test/host/wifi_reconnect_policy_test.cpp`
  - Cover the two reported recovery failures and gateway false-positive protection.
- Modify: `lib/AppSupport/ConfigPortal/WiFiPortalService.h`
  - Add asynchronous gateway-probe state and callback declarations.
- Modify: `lib/AppSupport/ConfigPortal/WiFiPortalService.cpp`
  - Disable competing Arduino auto-reconnect, protect DHCP progress, and run the gateway monitor.
- Create: `version.txt`
  - Pin the ESP-IDF application descriptor to the release version.
- Modify: `platformio.ini`, `README.md`, `docs/web-install/manifest.json`
  - Bump user-facing release metadata to 1.15.13.
- Create: `docs/web-install/firmware/firmware_1.15.13.zip`
  - Package the freshly built application and LittleFS images.

### Task 1: Extend The Host-Tested Reconnect Policy

- [ ] Add failing tests for the ten-second DHCP guard and first-versus-repeated disconnect retry state.
- [ ] Add failing tests for unproven gateways, arming on success, three post-baseline failures, recovery, and lease reset.
- [ ] Compile the test and confirm it fails because the new policy API does not exist.
- [ ] Implement the minimal pure policy helpers.
- [ ] Recompile and confirm the complete reconnect policy test passes.

### Task 2: Integrate The Runtime Reconnect Fix

- [ ] Disable WiFiManager and Arduino automatic reconnect so the bridge owns reconnect timing.
- [ ] Treat `STA_CONNECTED` and an explicit reconnect command as an in-flight DHCP interval.
- [ ] Preserve retry timing across repeated `STA_DISCONNECTED` callbacks.
- [ ] Clear reconnect state only on `GOT_IP` or after the DHCP guard expires.
- [ ] Build the application to catch ESP32 integration errors.

### Task 3: Add Gateway Half-Open Detection

- [ ] Add one-packet asynchronous gateway probes with a 30-second interval and one-second timeout.
- [ ] Ignore failures until one probe has succeeded for the current IP lease.
- [ ] Count three consecutive failures after that baseline before forcing station recovery.
- [ ] Ignore callbacks from a previous lease and treat ping setup failures as diagnostics only.
- [ ] Reset gateway-monitor enforcement on disconnect and new `GOT_IP`.
- [ ] Re-run host tests and the firmware build.

### Task 4: Prepare And Verify Release 1.15.13

- [ ] Bump `platformio.ini`, README, and web installer manifest versions.
- [ ] Add `version.txt` so the embedded ESP-IDF app descriptor reports exactly 1.15.13.
- [ ] Run the relevant host test, PlatformIO `buildprog`, and PlatformIO `buildfs` from a clean source state.
- [ ] Inspect the binary version strings, generated ZIP contents, checksums, and `git diff --check`.
- [ ] Self-review the complete diff and confirm the unrelated bytecode file remains untouched.
- [ ] Commit and push `main`.

### Task 5: Publish And Communicate

- [ ] Create GitHub release 1.15.13 with the generated OTA ZIP and detailed release notes.
- [ ] Verify the release tag, target commit, asset name, and published checksum.
- [ ] Post a detailed issue #21 reply in André's established style, explaining both failure modes and investigation limits.
- [ ] State clearly that physical bridge testing was not possible because André is on holiday, and ask the reporter to test the release.
- [ ] Verify the issue comment URL and public release URL.

