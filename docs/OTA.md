# RadPro WiFi Bridge OTA Updates

This firmware adds a dedicated **Firmware Update** page in the Wi-Fi portal so you can keep your bridge up to date without a USB cable. Two workflows are available and both are driven from `/ota` in the captive portal.

## 1. Update via Internet

1. Connect the bridge to Wi-Fi and open the configuration portal.
2. Navigate to **Firmware Update** and review the *Current* versus *Latest* versions. The bridge fetches the latest `manifest.json` from `https://sunbox.github.io/radpro-wifi-bridge/web-install/` (GitHub Pages) whenever the page is opened.
3. Press **Download & Install Latest Build**. The ESP32 starts a background task that
   - downloads the manifest,
   - retrieves every `.bin` listed in the manifest (`bootloader.bin`, `partitions.bin`, `radpro-wifi-bridge.bin`, `littlefs.bin`, …), and
   - flashes each part at the offset defined in the manifest via `esp_flash_write()`.
4. While the download runs the progress indicator shows the active part and bytes written. You can abort with **Cancel Update**.
5. After every part is written the bridge remounts LittleFS, marks the OTA as successful, and reboots automatically so the new firmware (and LittleFS content) takes effect.

> **Tip:** Keep the portal window open during the update so you can see status messages (HTTP downloads, flash progress, or errors such as Wi-Fi disconnects).

## 2. Manual ZIP Upload

If the bridge cannot reach GitHub Pages you can upload the firmware bundle yourself.

1. Build or download a bundle that contains:
   - `manifest.json` (same layout as the web installer) and
   - every file referenced in `manifest.builds[0].parts` (e.g. `firmware/latest/bootloader.bin`, `firmware/latest/partitions.bin`, `firmware/latest/radpro-wifi-bridge.bin`, `firmware/latest/littlefs.bin`).
2. Pack those files into a ZIP archive while preserving the relative paths listed in the manifest.
3. Open **Firmware Update** in the Wi-Fi portal, select the ZIP, and click **Upload Firmware ZIP**.
4. The browser unpacks the archive with JSZip, streams `manifest.json` to the ESP32, and uploads every binary in 16 KB base64 chunks over HTTPS (`/ota/upload/*` endpoints). Progress and errors are shown on the page.
5. When all parts are written the bridge finishes the OTA session, remounts LittleFS, and reboots automatically.

### ZIP Bundle Checklist

| Required file                | Description                          |
|------------------------------|--------------------------------------|
| `manifest.json`              | Defines the version and flash layout |
| `firmware/latest/bootloader.bin` | Bootloader image                    |
| `firmware/latest/partitions.bin` | Partition table                     |
| `firmware/latest/radpro-wifi-bridge.bin` | Main firmware image       |
| `firmware/latest/littlefs.bin` | Web-portal/LittleFS image            |

Add additional parts if the manifest references them. The uploader verifies that every path in the manifest exists inside the ZIP before flashing.

## Safety Notes

- OTA operations unmount LittleFS while `littlefs.bin` is written. Static pages might fail to load during the update—this is expected. The filesystem is automatically remounted once the OTA finishes or is aborted.
- All flash writes use offsets taken from the manifest, so keep the file paths and offsets in sync whenever you publish a new bundle.
- If anything goes wrong you can press **Cancel Update** to abort, remount LittleFS, and return the bridge to its previous state.
