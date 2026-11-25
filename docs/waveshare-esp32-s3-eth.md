# Waveshare ESP32-S3-ETH / W5500

RadPro WiFi Bridge can be built for the Waveshare ESP32-S3-ETH as an optional target. The existing ESP32-S3 DevKitC target remains the default and uses the same upstream toolchain and behavior as before.

## Build target

Use the dedicated PlatformIO environment:

```text
waveshare-esp32-s3-eth
```

For example:

```sh
pio run -e waveshare-esp32-s3-eth
```

The target inherits the upstream `espressif32@6.12.0` Arduino + ESP-IDF environment. W5500 support is enabled only through `sdkconfig.waveshare.defaults`; the default DevKitC environment remains unchanged.

## Network behavior

On the Waveshare target:

1. The onboard W5500 is initialized before Wi-Fi.
2. If Ethernet obtains a DHCP address, Ethernet is used and Wi-Fi remains off.
3. The existing configuration portal and application routes are served on the Ethernet address.
4. MQTT, time synchronization, OTA, diagnostics, and supported cloud publishers accept either Ethernet or Wi-Fi connectivity.
5. If Ethernet is unavailable, the existing WiFiManager flow is used as the fallback.
6. If Ethernet becomes available later, it takes over and the Wi-Fi radio is shut down.
7. If Ethernet is subsequently lost, Wi-Fi fallback can start again.

## Onboard W5500 pin mapping

| Signal | GPIO |
| --- | ---: |
| SCLK | 13 |
| MISO | 12 |
| MOSI | 11 |
| CS | 14 |
| RESET | 9 |
| INT | 10 |

The Waveshare board's onboard RGB LED is GPIO 21.

Ethernet currently uses DHCP.
