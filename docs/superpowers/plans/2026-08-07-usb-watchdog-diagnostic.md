# USB Watchdog Diagnostic Build Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish a `1.15.14-test.1` prerelease that disables only the secondary USB descriptor client so issue #21's reporter can test the suspected USB open/close race.

**Architecture:** Put the compile-time choice in a pure C++ policy header that defaults to the existing enabled behavior. Gate descriptor-client registration in `UsbCdcHost::begin()`, disable it only on the diagnostic branch, and package verified ESP32-S3 images without changing the stable release or web installer.

**Tech Stack:** C++17 host tests, Arduino-ESP32 2.0.17, ESP-IDF 4.4.7, PlatformIO, Git, GitHub CLI.

## Global Constraints

- Change one runtime variable only: whether the secondary descriptor-inspection client starts.
- Keep the actual CDC driver, Wi-Fi logic, watchdog configuration, and task priorities unchanged.
- Identify the diagnostic firmware as `1.15.14-test.1`.
- Publish as a GitHub prerelease, not as the latest stable release.
- Do not claim the USB problem is fixed until the reporter confirms the A/B result on hardware.
- Preserve the unrelated Python bytecode modification in the main checkout.

---

### Task 1: Add the Compile-Time Descriptor Client Policy

**Files:**
- Create: `lib/UsbCdcHost/UsbDescriptorClientPolicy.h`
- Create: `test/host/usb_descriptor_client_policy_test.cpp`

**Interfaces:**
- Produces: `UsbDescriptorClientPolicy::isEnabled() -> constexpr bool`
- Consumes: optional `RADPRO_USB_DESCRIPTOR_CLIENT_ENABLED` preprocessor value, defaulting to `1`

- [ ] **Step 1: Write the failing host test**

```cpp
#include <cassert>
#include <iostream>
#include "UsbDescriptorClientPolicy.h"

#ifndef EXPECT_USB_DESCRIPTOR_CLIENT_ENABLED
#error "EXPECT_USB_DESCRIPTOR_CLIENT_ENABLED must be set"
#endif

int main()
{
    assert(UsbDescriptorClientPolicy::isEnabled() ==
           static_cast<bool>(EXPECT_USB_DESCRIPTOR_CLIENT_ENABLED));
    std::cout << "usb descriptor client policy tests passed\n";
}
```

- [ ] **Step 2: Verify RED in both configurations**

Run:

```bash
c++ -std=c++17 -Ilib/UsbCdcHost -DEXPECT_USB_DESCRIPTOR_CLIENT_ENABLED=1 test/host/usb_descriptor_client_policy_test.cpp -o /tmp/usb_descriptor_default_test
```

Expected: compilation fails because `UsbDescriptorClientPolicy.h` does not exist.

- [ ] **Step 3: Implement the minimal policy**

```cpp
#pragma once

#ifndef RADPRO_USB_DESCRIPTOR_CLIENT_ENABLED
#define RADPRO_USB_DESCRIPTOR_CLIENT_ENABLED 1
#endif

namespace UsbDescriptorClientPolicy
{
constexpr bool isEnabled()
{
    return RADPRO_USB_DESCRIPTOR_CLIENT_ENABLED != 0;
}
}
```

- [ ] **Step 4: Verify GREEN in the default and diagnostic configurations**

Run:

```bash
c++ -std=c++17 -Ilib/UsbCdcHost -DEXPECT_USB_DESCRIPTOR_CLIENT_ENABLED=1 test/host/usb_descriptor_client_policy_test.cpp -o /tmp/usb_descriptor_default_test
/tmp/usb_descriptor_default_test
c++ -std=c++17 -Ilib/UsbCdcHost -DRADPRO_USB_DESCRIPTOR_CLIENT_ENABLED=0 -DEXPECT_USB_DESCRIPTOR_CLIENT_ENABLED=0 test/host/usb_descriptor_client_policy_test.cpp -o /tmp/usb_descriptor_disabled_test
/tmp/usb_descriptor_disabled_test
```

Expected: both print `usb descriptor client policy tests passed` and exit zero.

### Task 2: Gate the Secondary USB Client and Label the Diagnostic Firmware

**Files:**
- Modify: `lib/UsbCdcHost/UsbCdcHost.cpp`
- Modify: `platformio.ini`
- Modify: `version.txt`

**Interfaces:**
- Consumes: `UsbDescriptorClientPolicy::isEnabled()`
- Preserves: all CDC driver and task creation behavior

- [ ] **Step 1: Include the policy and guard only descriptor-client registration**

Add `#include "UsbDescriptorClientPolicy.h"` and wrap the existing `usb_host_client_register` plus `usb_dbg` task creation in:

```cpp
if (UsbDescriptorClientPolicy::isEnabled())
{
    // existing descriptor client registration
}
else
{
    ESP_LOGW(TAG, "USB descriptor client disabled for diagnostic build");
}
```

- [ ] **Step 2: Select the diagnostic mode and version**

Change `BRIDGE_FIRMWARE_VERSION` and `version.txt` to `1.15.14-test.1`, and add:

```ini
-DRADPRO_USB_DESCRIPTOR_CLIENT_ENABLED=0
```

- [ ] **Step 3: Re-run all four USB host tests**

Compile and run `usb_descriptor_client_policy_test.cpp` in both modes plus `usb_attach_delay_policy_test.cpp`, `usb_diagnostic_messages_test.cpp`, and `usb_recovery_policy_test.cpp`.

Expected: all tests exit zero.

### Task 3: Build and Verify the Diagnostic Package

**Files:**
- Build output: `.pio/build/esp32-s3-devkitc-1/firmware.bin`
- Build output: `.pio/build/esp32-s3-devkitc-1/littlefs.bin`
- Create outside repository: `/tmp/radpro-wifi-bridge-1.15.14-test.1.zip`

**Interfaces:**
- Consumes: PlatformIO ESP32-S3 build environment
- Produces: one diagnostic ZIP with `manifest.json`, `bootloader.bin`, `partitions.bin`, `radpro-wifi-bridge.bin`, and `littlefs.bin`

- [ ] **Step 1: Build the application and LittleFS images**

Run:

```bash
$HOME/.platformio/penv/bin/platformio run -e esp32-s3-devkitc-1 -t buildprog
$HOME/.platformio/penv/bin/platformio run -e esp32-s3-devkitc-1 -t buildfs
```

Expected: both commands finish with `SUCCESS`.

- [ ] **Step 2: Verify embedded version and diagnostic flag**

Inspect the application image and build command database for `1.15.14-test.1` and `RADPRO_USB_DESCRIPTOR_CLIENT_ENABLED=0`.

- [ ] **Step 3: Package without updating the stable web installer**

Create a temporary manifest derived from the existing layout with version `1.15.14-test.1`, then ZIP the freshly built images under the same relative paths used by the web installer.

- [ ] **Step 4: Verify ZIP structure and hashes**

Extract the ZIP to a temporary directory and compare SHA-256 hashes against every source image. Expected: five entries and four binary hash matches.

### Task 4: Publish the Diagnostic and Reply to Issue #21

**Files:**
- Commit the spec, plan, policy, test, USB gate, and diagnostic version metadata.
- Do not commit generated web-installer binaries.

**Interfaces:**
- Produces: branch `diagnostic/issue-21-usb-watchdog`, prerelease tag `1.15.14-test.1`, and an owner-style issue comment

- [ ] **Step 1: Review and commit the diagnostic source**

Run `git diff --check`, inspect the complete diff, and commit with a concise diagnostic message.

- [ ] **Step 2: Push the diagnostic branch**

Push `diagnostic/issue-21-usb-watchdog` to `origin` without modifying `main`.

- [ ] **Step 3: Create the prerelease**

Create tag/release `1.15.14-test.1` targeting the pushed diagnostic commit, mark it `--prerelease`, attach the verified ZIP, and explain that it tests only the secondary USB client hypothesis.

- [ ] **Step 4: Verify stable-release status**

Confirm `1.15.14-test.1` is a prerelease and `1.15.13` remains the latest stable release.

- [ ] **Step 5: Post the issue reply**

Write in André's established short maintainer style: thank the reporter, explain why this is a task-watchdog reset during USB enumeration rather than a boot watchdog problem, link the prerelease, state exactly what changed, ask for repeated boot/USB-attach testing plus a continuous log, and clearly say it is not yet a confirmed fix.

- [ ] **Step 6: Verify the public links**

Read back the release and newest issue comment and confirm their URLs, text, target commit, prerelease state, and asset digest.
