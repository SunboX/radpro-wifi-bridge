# RadPro WiFi Bridge (ESP32‑S3)

A tiny Wi‑Fi/USB bridge for Rad Pro–class devices built on **ESP32‑S3 (Arduino)**.  
It enumerates USB devices via TinyUSB **host**, provides a simple non‑blocking startup flow you can control over serial, and exposes a visual heartbeat on the on‑board **WS2812 RGB LED**.

> **Planned reporting targets:** This firmware will be able to publish measurements to well‑known platforms like **openSenseMap**, **Home Assistant** (via MQTT), and other common stacks (e.g., MQTT brokers / InfluxDB / Grafana via your backend).

---

## Features

- **USB Host (TinyUSB):** Enumerates attached USB devices and logs VID/PID, class, and basics.
- **Dual Serial I/O:**  
  - `Serial` → native USB‑CDC (TinyUSB).  
  - `Serial0` (**recommended for debug**) → the board’s CP210x / “second USB port”.
- **Non‑blocking startup state machine:** Default delay is **5 s**. You can:
  - start immediately with `start`
  - change the delay with `delay <ms>`
- **On‑board RGB LED heartbeat (WS2812):**
  - Boot cue: dim green.
  - Main loop: dim‑green pulse every 250 ms.
  - Uses `neopixelWrite(RGB_BUILTIN, r, g, b)`; default `RGB_BUILTIN = 48`.

---

## Quick Start

1. Open the project in **PlatformIO** (VS Code).
2. Ensure the environment targets **ESP32‑S3 DevKitC‑1 (N16R8)** and that TinyUSB host is enabled (see `platformio.ini` in this repo).
3. Connect **both** USB ports if possible:
   - Use the **CP210x / second USB port** for logs and runtime commands (`Serial0` @ **115200**).
   - The **native USB‑OTG** is used by TinyUSB host and should be left free for devices.
4. Build & upload.
5. Open a serial monitor on the CP210x port and watch the countdown or send commands.

> macOS tip: `ls /dev/tty.*` then e.g. `screen /dev/tty.usbserial-* 115200` (to exit `screen`, press `Ctrl+A`, then `K`, then `Y`).

---

## Runtime Behavior

### Startup & Commands (via `Serial0`)
| Command           | Description                                 | Example              |
|-------------------|---------------------------------------------|----------------------|
| `start`           | Starts the main loop immediately             | `start`              |
| `delay <ms>`      | Sets a new startup delay and restarts timer | `delay 15000`        |

While waiting, the device prints a **countdown** once per second. After starting, it logs “Main loop is running.” every second.

### LED Behavior
- **Boot:** brief green pulse
- **Running:** dim green heartbeat every **250 ms**

Change the LED pin by redefining `RGB_BUILTIN` in your build flags or sketch:
```cpp
#define RGB_BUILTIN 48  // default ESP32‑S3 DevKitC‑1 WS2812 pin
```

---

## File Overview

- `src/main.cpp` – Arduino sketch with:
  - USB host setup (library task + client task)
  - Startup state machine (non‑blocking)
  - RGB LED heartbeat (WS2812 via `neopixelWrite`)
- `platformio.ini` – ESP32‑S3 DevKitC‑1 (N16R8) config with TinyUSB **host** enabled and UART debug on `Serial0`.

---

## Roadmap

- Publish live CPM / pulse counts to:
  - **openSenseMap**
  - **Home Assistant** (via MQTT / discovery)
  - Generic **MQTT** (for custom pipelines like InfluxDB/Grafana)
- Configurable reporting intervals & filters
- OTA updates

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
