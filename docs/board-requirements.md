# Board Requirements

This project no longer depends on the discontinued
`ESP32-S3-DevKitC-1-N16R8V`. The default PlatformIO profile now targets the
currently common `ESP32-S3-DevKitC-1-N16R8`, and the same firmware layout also
fits `N16R8V` boards.

## Recommended Board

- `ESP32-S3-DevKitC-1-N16R8`
- `16 MB` flash
- `8 MB` PSRAM
- two USB connectors so you can keep the OTG port free for the detector and use
  the second port for flashing/logs
- onboard WS2812 on `GPIO48`

## Minimum Requirements For The Default Build

If you want to compile this repository without changing the partition layout,
your board should provide:

- an `ESP32-S3` with native USB OTG host support
- routing that allows the OTG port to connect to the detector after the
  documented solder bridges are closed
- `16 MB` flash
- a way to flash firmware and read logs while the detector uses the OTG port
  (a second USB/UART path is the easiest option)

## What Is Optional

- PSRAM is currently optional for this firmware. The current codebase builds and
  links without enabling PSRAM-specific support.
- An onboard WS2812 is recommended because the firmware uses it for status, but
  you can adapt the LED wiring if your board exposes a different RGB LED or no
  LED at all.
- The exact DevKitC module suffix is not critical as long as the board meets the
  flash, USB-OTG, and routing requirements.

## If You Want To Use A Smaller Board

Boards with less than `16 MB` flash are not supported by the default layout.
The repository currently reserves:

- two OTA application slots (`ota_0`, `ota_1`)
- a `2 MB` LittleFS partition for the web portal assets

If you want to target an `8 MB` or smaller board, you need to provide your own:

- smaller partition table
- reduced OTA strategy or a single-app layout
- matching web installer offsets if you also want to keep browser flashing

That is possible, but it is a separate board-porting task rather than the
default configuration.
