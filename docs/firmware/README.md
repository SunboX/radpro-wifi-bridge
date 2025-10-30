# Firmware bundle placeholder

Files in this directory are published to GitHub Pages and consumed by the ESP Web Tools installer.

After every successful PlatformIO build, the helper `tools/copy_firmware.py` mirrors the generated artefacts into `docs/firmware/latest/` with the correct filenames:

- `.pio/build/esp32-s3-devkitc-1/bootloader.bin` → `docs/firmware/latest/bootloader.bin`
- `.pio/build/esp32-s3-devkitc-1/partitions.bin` → `docs/firmware/latest/partitions.bin`
- `.pio/build/esp32-s3-devkitc-1/firmware.bin` → `docs/firmware/latest/radpro-wifi-bridge.bin`

If you build outside of PlatformIO or need to republish manually, make sure the filenames and offsets continue to match the entries in `docs/manifest.json`. Update the manifest `version` field whenever you cut a new firmware release.
