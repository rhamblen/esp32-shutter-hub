# v0.0.1 — Firmware framework + OTA scaffold

First tagged release. Alongside the full design documentation, this ships the **first buildable
firmware**: a minimal ESP32 base image with **on-device WiFi setup**, a status page, and
**over-the-air firmware updates** in the browser. Servos, HomeKit, MQTT and solar logic bolt onto
this in later phases — it runs on a bare ESP32 dev board with no other hardware.

## What it does
- **On-device WiFi setup** — first boot brings up an access point `Shutter-Hub-Setup` with a
  captive portal; pick your network and it's saved to NVS. No credentials in the binary, and the
  saved WiFi **survives OTA updates**. (WiFiManager — the Arduino-native version of the SoftAP +
  captive-portal pattern in [HomeKey-ESP32](https://github.com/rednblkx/HomeKey-ESP32).)
- Advertises mDNS `shutter-hub.local` and serves a status page (WiFi, IP, MAC, RSSI, uptime, heap).
- Browser OTA at **`/update`** (ElegantOTA + ESPAsyncWebServer).

## Download — pick your board

| Board | First flash over USB (offset `0x0`) | Update over the air (`/update`) |
| ----- | ----------------------------------- | ------------------------------- |
| **ESP32-D** (WROOM DevKit) | `shutter-hub-esp32d-full-v0.0.1.bin` | `shutter-hub-esp32d-ota-v0.0.1.bin` |
| **ESP32-C3** (DevKitM) | `shutter-hub-esp32c3-full-v0.0.1.bin` | `shutter-hub-esp32c3-ota-v0.0.1.bin` |

- **`-full-`** — complete image (bootloader + partitions + app). Flash once over USB at offset `0x0`
  with esptool or NodeMCU-PyFlasher.
- **`-ota-`** — app only. Upload from the device's `/update` page for every update after the first.

> The bins contain **no WiFi credentials** — each board is configured on-device via the
> `Shutter-Hub-Setup` portal, so the same image works on any network. Build from source with
> PlatformIO (`cd firmware && pio run`) — both board targets are in `platformio.ini`.

## Flash it (first time, USB)
```
esptool --chip esp32   write_flash 0x0 shutter-hub-esp32d-full-v0.0.1.bin     # ESP32-D
esptool --chip esp32c3 write_flash 0x0 shutter-hub-esp32c3-full-v0.0.1.bin    # ESP32-C3
```
Then open `http://shutter-hub.local/` (or the device IP) and use `/update` from then on.

## Build from source
PlatformIO project in [`firmware/`](firmware/) — `pio run` builds both board targets. See
[`firmware/README.md`](firmware/README.md).

Full details in [CHANGELOG.md](CHANGELOG.md).
