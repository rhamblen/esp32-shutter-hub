# v0.0.2 — Skeleton complete: tabbed UI, config store, diagnostics

Completes the firmware framework on top of v0.0.1. The device now has a tabbed web UI, a
persisted settings store, structured diagnostics, and a modular codebase ready for the real
feature phases. Still runs on a bare ESP32-D — no servo/PCA9685 hardware required.

> Supersedes v0.0.1 (which also included a captive-portal responsiveness fix rolled in here).

## What's new since v0.0.1
- **Tabbed web UI** — **Network** (connection info, Change WiFi, Reboot, `/info`), **Firmware**
  (custom OTA, below), **Apple Home** (blank placeholder for the Phase 5 HomeKit UI).
- **Custom OTA page** — shows the installed version, takes a firmware image and a LittleFS
  filesystem image in separate boxes (flash either independently or both together), and records the
  last flash (what / when / result). Replaces ElegantOTA.
- **On-device WiFi setup** stays snappy (WiFi modem power-save disabled) and, once WiFi is saved,
  the `Shutter-Hub-Setup` access point is **switched off**. A setting (default off) + a Network-tab
  button can re-enable it as a management AP at `192.168.4.1`.
- **Settings store** (`AppConfig`, NVS) and **Diagnostics** — structured serial logging, a `/info`
  JSON endpoint, and a Reboot control.
- **Modular firmware** — thin `main.cpp` wiring real modules plus stubs for the servo/MQTT/HomeKit/
  light-sensor phases.

## Download (ESP32-D)
| First flash over USB (offset `0x0`) | Update over the air |
| ----------------------------------- | ------------------- |
| `shutter-hub-esp32d-full-v0.0.2.bin` | `shutter-hub-esp32d-ota-v0.0.2.bin` |

_ESP32-C3 bins are deferred to a later release._

## Flash it
First time, over USB:
```
esptool --chip esp32 write_flash 0x0 shutter-hub-esp32d-full-v0.0.2.bin
```
Then join `Shutter-Hub-Setup`, pick your network, and open `http://shutter-hub.local/`. After that,
update over the air from the **Firmware** tab — select the `-ota-` bin in the **Firmware image** box
and click Flash (leave the Filesystem box empty; no LittleFS image ships yet). Saved WiFi and
settings live in NVS and survive updates.

## Build from source
PlatformIO project in [`firmware/`](firmware/) — `pio run` builds the ESP32-D target. See
[`firmware/README.md`](firmware/README.md). Full history in [CHANGELOG.md](CHANGELOG.md).
