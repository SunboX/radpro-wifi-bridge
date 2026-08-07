# USB Watchdog Diagnostic Build Design

## Purpose

Issue #21 now contains a second failure mode: firmware 1.15.13 finishes Wi-Fi and MQTT startup, enumerates the detector's USB CDC descriptors, and then resets with `TG0WDT_SYS_RST`. The diagnostic build must test the single leading hypothesis that the separate descriptor-inspection USB client overlaps with the CDC driver's attempt to open the same device.

This build is an experiment, not a stable bug-fix release. Physical confirmation from the reporter is required before changing the production USB lifecycle.

## Considered Approaches

1. **Disable only the descriptor-inspection client in a diagnostic build.** This changes one runtime variable, preserves the actual CDC connection path, and gives a clear A/B result. This is the selected approach.
2. **Immediately serialize descriptor inspection and CDC opening.** This could become the final fix, but implementing it before the hypothesis is confirmed would mix diagnosis with repair.
3. **Increase watchdog timeouts or change task priorities.** This would mask the lockup and would not test the suspected USB ownership race, so it is rejected.

## Design

Add a small compile-time policy with descriptor inspection enabled by default. `UsbCdcHost::begin()` will register and start the secondary descriptor client only when that policy is enabled. The diagnostic branch will compile it disabled and will identify itself as `1.15.14-test.1`.

The normal default remains enabled, so the source change cannot silently alter future production builds. A host test will compile and run the policy in both modes, proving that the default and diagnostic selections are distinct.

The diagnostic package will contain fresh ESP32-S3 application and LittleFS images plus the unchanged bootloader and partition images required by the existing OTA package format. It will be published as a GitHub prerelease with a clear experimental label. The stable `1.15.13` release and web installer will not be replaced.

## Reporter Test

The issue reply will explain that the new logs point to the USB phase rather than the boot or Wi-Fi watchdog. The reporter will be asked to flash the diagnostic package, repeat the boot/USB-attach conditions that produced the reset loop, and provide the continuous serial log plus whether any `TG0WDT_SYS_RST` resets remain.

## Success Criteria

- The compile-time policy test fails before implementation and passes afterward in enabled and disabled modes.
- Existing USB host policy tests still pass.
- The ESP32-S3 application and LittleFS builds complete.
- The embedded application version reports `1.15.14-test.1`.
- The diagnostic ZIP contains the expected four images and their hashes match the built inputs.
- The GitHub entry is marked as a prerelease and does not replace `1.15.13` as the latest stable release.
- The issue comment links the diagnostic prerelease and does not claim a confirmed fix.
