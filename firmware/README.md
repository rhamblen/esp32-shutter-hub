# Firmware — ESP32 Smart Shutter Hub

PlatformIO project (Arduino Core, `esp32dev`). Structural reference:
[HomeKey-ESP32](https://github.com/rednblkx/HomeKey-ESP32). See
[../docs/project-plan.md](../docs/project-plan.md) for the phased roadmap.

## What this version does (v0.0.1 — framework scaffold)

On-device WiFi setup, advertises `shutter-hub.local` over mDNS, serves a status
page, and accepts **over-the-air firmware updates at `/update`** (ElegantOTA). No
servo, PCA9685 or power hardware required — it runs on a bare ESP32 dev board.
This is the base every later phase (servos, HomeKit, MQTT, solar) builds on.

## WiFi setup (on-device — no credentials in the binary)

On first boot (and after **Change WiFi**) the hub becomes an access point
**`Shutter-Hub-Setup`** with a captive portal:

1. Join the `Shutter-Hub-Setup` network from a phone or laptop.
2. The captive portal opens (or browse to `http://192.168.4.1`). Pick your WiFi
   and enter the password.
3. The hub saves the credentials to **NVS** and reconnects. NVS is a separate
   flash partition, so the credentials **survive OTA updates** — you only do this
   once.

This uses [WiFiManager](https://github.com/tzapu/WiFiManager) — the Arduino-native
version of the SoftAP + captive-portal pattern in
[HomeKey-ESP32](https://github.com/rednblkx/HomeKey-ESP32). Because nothing is
compiled in, the same prebuilt bin works on anyone's network. To change networks
later, hit **Change WiFi** on the status page (or hold the reset long enough for
the portal to reappear).

## Build

Two board targets are defined: `esp32dev` (ESP32-D, primary) and `esp32-c3-devkitm-1` (ESP32-C3).

```
pio run                            # builds BOTH targets
pio run -e esp32dev                # just the ESP32-D
pio run -e esp32-c3-devkitm-1      # just the ESP32-C3
```

Prebuilt bins for both boards are collected in [`dist/`](dist/):

| Board | First USB flash (merged, `0x0`) | OTA (`/update`) |
| ----- | ------------------------------- | --------------- |
| ESP32-D  | `shutter-hub-esp32d-full-vX.Y.Z.bin`  | `shutter-hub-esp32d-ota-vX.Y.Z.bin`  |
| ESP32-C3 | `shutter-hub-esp32c3-full-vX.Y.Z.bin` | `shutter-hub-esp32c3-ota-vX.Y.Z.bin` |

## First flash (USB — one time only)

After this, every update goes over WiFi (see OTA below).

**Option A — PlatformIO, board on USB:**
```
pio run -e esp32dev -t upload      # auto-detects the port; monitor with:  pio device monitor
```

**Option B — single merged image (NodeMCU-PyFlasher / esptool):**
Flash the `...-full-...bin` for your board at offset `0x0`:
```
esptool --chip esp32   write_flash 0x0 dist/shutter-hub-esp32d-full-v0.0.1.bin    # ESP32-D
esptool --chip esp32c3 write_flash 0x0 dist/shutter-hub-esp32c3-full-v0.0.1.bin   # ESP32-C3
```
In NodeMCU-PyFlasher: select the `...-full-...bin`, address `0x0`, flash.

> Merge recipe (how the `-full-` images are made): `esptool --chip <chip> merge-bin`
> with bootloader + partitions + boot_app0 + firmware. **ESP32-D bootloader sits at `0x1000`,
> ESP32-C3 bootloader at `0x0`** — the app is always at `0x10000`.

## Update over the air (every time after the first)

1. Build a new bin (`pio run`), or grab `...-ota-...bin` from a release.
2. Browse to `http://shutter-hub.local/update` (or the device IP).
3. Upload the matching board's `...-ota-...bin`. The board reboots into the new
   build — your saved WiFi is untouched (it lives in NVS, not the app partition).

## Layout

```
firmware/
├─ platformio.ini              board targets + libraries + build flags
├─ src/
│  └─ main.cpp                 WiFiManager + mDNS + AsyncWebServer + ElegantOTA
├─ dist/                       prebuilt bins (gitignored; attached to releases)
└─ README.md
```

Future phases add `ServoController`, `Config`, `Mqtt`, `HomeKit`, `LightSensor`,
`Diagnostics` modules and a LittleFS `data/` web UI (see ADR 0004).
