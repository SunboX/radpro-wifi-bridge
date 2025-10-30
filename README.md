# RadPro WiFi Bridge (ESP32‑S3)

Wi‑Fi / USB bridge firmware for **Bosean RadPro (FS‑600)‑class** Geiger counters.  
The ESP32‑S3 enumerates the detector as a vendor‑specific CDC device, exposes status over serial, keeps a live heartbeat on the on‑board WS2812 LED, and mirrors data to the network. Configuration happens through a captive portal or a small web UI served directly by the board.

---

## Highlights

- **TinyUSB host stack** with a CH34x VCP shim so the RadPro enumerates reliably.
- **DeviceManager queue** that boots the tube, captures `deviceId`/model/firmware, and keeps polling live readings (`GET tubePulseCount`, `GET tubeRate`, …).
- **Wi‑Fi configuration portal** (WiFiManager based):
  - On first boot (or when credentials fail) an AP is launched for setup.
  - After connecting to WLAN the same form is available at `http://<device-ip>/`.
  - Saved settings (device name, MQTT parameters, polling interval) are stored in NVS.
- **Runtime Wi‑Fi logging**: SSID/IP/gateway and RSSI are printed on connect, disconnect reasons are noted.
- **Non‑blocking startup state machine**: default 5 s delay with serial overrides.
- **Serial control** on the CP210x debug port (`Serial0` @ 115200) with optional raw USB tracing.
- **RGB LED heartbeat** (WS2812 on GPIO 48) showing boot and runtime status.

---

## Quick Start

1. Install [PlatformIO](https://platformio.org/) and open the project.
2. The default environment targets **ESP32‑S3 DevKitC‑1 (N16R8)** with TinyUSB host enabled (see `platformio.ini`).
3. Connect the board:
   - **CP210x USB (UART)** for logs / commands (`Serial0` 115200 baud).
   - Keep the **native USB‑OTG** port free for the RadPro sensor.
4. Flash: `platformio run --target upload`.
5. Open a serial monitor on the CP210x port to watch the countdown or enter commands.

> macOS: `ls /dev/tty.*` then e.g. `screen /dev/tty.usbserial-* 115200` (to exit `screen`, press `Ctrl+A`, then `K`, then `Y`).

When no Wi‑Fi credentials are stored the firmware opens a captive portal (`RadPro WiFi Bridge Setup`). Once credentials are provided it reconnects as a station and logs the assigned IP.

---

## Serial Console Commands (`Serial0`)

| Command        | Description                                              |
|----------------|----------------------------------------------------------|
| `start`        | Skip the remaining boot delay and start immediately.     |
| `delay <ms>`   | Set a new startup delay (milliseconds) and restart timer.|
| `raw on/off`   | Enable or disable raw USB framing logs.                  |
| `raw toggle`   | Toggle raw USB logging.                                  |

While waiting for the boot delay the console prints `Starting in …` once per second. After the bridge starts, only device data and Wi‑Fi status are logged (the old “Main loop is running.” message was removed to keep the console clean).

Raw USB logging is useful for troubleshooting atypical firmware responses. Disable it for normal operation to preserve bandwidth.

---

## Wi‑Fi Configuration

- **Captive portal:** launched automatically if auto‑connect fails or no credentials exist. Device parameters (name, MQTT host/user/pass/topic, RadPro polling interval) are editable here.
- **Station portal:** once connected, browse to `http://<device-ip>/` to reopen the same form. Settings are saved in NVS.
- **Status logging:** on every connect the firmware prints SSID, IP, gateway, mask, and RSSI. Disconnect reasons (e.g., `AUTH_FAIL`) are emitted once per event.

Configuration is managed by `WiFiPortalService`, which continuously maintains the web portal while the main loop runs.

---

## Device Data Flow

1. **USB connection** → the RadPro enumerates (using CH34x VCP driver if needed).
2. **Initial handshake**:
   - `GET deviceId` → logs ID, model, firmware, locale/time zone.
   - Additional `GET` commands gather tube sensitivity, dead time, HV parameters, etc.
3. **Continuous polling**: `GET tubePulseCount` and `GET tubeRate` are queued at the configured interval (defaults to 1000 ms, clamped to 500 ms minimum).
4. **Raw logging** (optional) prints hex payloads for each USB frame.

Command retries and back‑off are handled automatically inside `DeviceManager`.

---

## LED Indicators

- **Boot:** dim green pulse during startup.
- **Running:** heartbeat pulses every 250 ms (adjust in `runMainLogic()`).

You can change the LED pin by redefining `RGB_BUILTIN` (default is `48` on the ESP32‑S3 DevKitC‑1):

```cpp
#define RGB_BUILTIN 48
```

---

## Project Structure

| Path                                   | Purpose                                                             |
|----------------------------------------|---------------------------------------------------------------------|
| `src/main.cpp`                         | Arduino entry point: USB host startup, Wi‑Fi portal orchestration, startup delay logic, LED heartbeat. |
| `lib/DeviceManager`                    | RadPro command queue, response parsing, automatic telemetry polling.|
| `lib/UsbCdcHost`                       | TinyUSB wrapper + CH34x helper to drive the vendor CDC interface.   |
| `lib/AppSupport/AppConfig`             | NVS-backed configuration storage (device name, MQTT settings, read interval). |
| `lib/AppSupport/ConfigPortal`          | WiFiManager-based captive portal and station web portal service.    |
| `platformio.ini`                       | PlatformIO configuration targeting ESP32‑S3 DevKitC‑1 with TinyUSB host. |

---

## Roadmap

- Publish CPM / pulse counts to:
  - **openSenseMap**
  - **Home Assistant** (MQTT discovery)
  - Generic **MQTT** / custom backends
- Configurable outbound reporting cadence & thresholds.
- OTA update support.

---

## License

This project is licensed under the **MIT License**.

```
MIT License

Copyright (c) 2025 RadPro WiFi Bridge contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
