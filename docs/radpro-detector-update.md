# RadPro detector firmware updates (experimental)

The **RadPro Detector Update** menu entry updates the connected radiation detector.
The existing **Firmware Update** entry updates the ESP32 bridge.

This implementation targets **Rad Pro 3.1.1**. It is not a published bridge release.
Flashing support is implemented but requires physical testing on each model.

| Detector | Through this bridge | Prerequisite / alternative |
| --- | --- | --- |
| Bosean FS-600, FS-1000, FS-5000 | Implemented; hardware validation pending | Stable Rad Pro 3.0.2+ (3.x), supported USB serial adapter |
| GQ GMC-800 | Implemented; hardware validation pending | Stable Rad Pro 3.0.2+ (3.x); CH340 DTR/RTS control required |
| FNIRSI GC-01 / JOY-IT JT-RAD01 | Unavailable | Upstream vendor USB drive installation procedure; hardware variants need different images |
| FNIRSI GC-03 | Unavailable | Upstream vendor USB drive installation procedure |
| FS2011 / YT-203B | Unavailable | External programmer and the hardware-specific installation guide |

The FNIRSI installation guides explicitly require Windows 10/11. Their bootloaders
are not standard serial STM32 ROM bootloaders. The current bridge only has a serial
USB host driver; telemetry compatibility does not imply flashing compatibility.
GQ stock-firmware installation, arbitrary custom binaries, test builds, readout
unprotection and factory reset are intentionally outside this updater's scope.

## Manual test

1. Install both the ESP32 firmware and its matching LittleFS web assets.
2. Open `/radpro-update` on the bridge and verify the detected model/firmware.
3. Choose a firmware language and download the linked official `.bin` file.
4. Select that same file and accept the Rad Pro terms and installation confirmation.
5. Click **Upload & install on detector**. Keep both devices powered and connected.
6. Wait for **Rad Pro 3.1.1 verified** and check that telemetry resumes.

The browser uploads small, ordered chunks to a LittleFS staging file. No detector
commands are sent by upload or cancel. The server checks the full file against a
compiled size/SHA-256 catalog before reserving USB or entering the bootloader.
The catalog pins 120 model/language images from the upstream installer at commit
`b76d6371ea48bad34e54a2bef237d34116b5607b`; these are hashes, not upstream signatures.
An image from another release, model, or language is rejected.

Before erasing, the bridge rechecks the detector identity, checks the STM32 product
ID and probes read access. It uses 57600 baud / 8E1, erases only the pages covered
by the firmware, writes blocks in reverse order (vector table last), and reads
every block back. No mass erase or readout-unprotect command is implemented.
The detector's settings and log pages are not deliberately erased, matching the
upstream installer's non-factory-reset flow; back up important data beforehand.

Telemetry and outgoing measurement publishers pause while flashing. USB recovery
restarts, bridge OTA, portal restart and application-settings POSTs are inhibited
during the relevant update state. The portal remains available for progress.

## Failure / recovery

An upload can be discarded before flashing. Incomplete uploads expire after two
minutes. Flashing cannot be cancelled after bootloader entry. Following an error,
the portal retains the verified staged file and offers **Retry staged firmware**
when the original USB connection is still present. Telemetry remains paused while
the detector may be in its bootloader. Never substitute a different detector on
that connection: a disconnect/reconnect invalidates the upload session.

If retry cannot recover the detector, connect it directly to a computer and use
the official web installer or the hardware-specific recovery guide. Bridge resets
or power loss during a detector update can require that external recovery route.

## Sources and validation

- [Rad Pro 3.1.1 release](https://github.com/Gissio/radpro/releases/tag/3.1.1)
- [Upstream serial installer and flash geometry](https://github.com/Gissio/radpro-installer/blob/b76d6371ea48bad34e54a2bef237d34116b5607b/index.html)
- [Device guides at 3.1.1](https://github.com/Gissio/radpro/tree/3.1.1/docs/devices)
- [Espressif CH34x control-line polarity](https://github.com/espressif/esp-usb/blob/master/host/class/cdc/usb_host_ch34x_vcp/usb_host_ch34x_vcp.c)

Run `python3 tools/run_host_tests.py` for native protocol/policy and portal workflow
checks, and build application and LittleFS with PlatformIO. Those checks do not
prove physical erase/write/reset behavior on the four detector models.
