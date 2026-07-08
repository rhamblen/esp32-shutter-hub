# Firmware — ESP32 Smart Shutter Hub

PlatformIO project (Arduino Core, `esp32dev`). Structural reference:
[HomeKey-ESP32](https://github.com/rednblkx/HomeKey-ESP32). See
[../docs/project-plan.md](../docs/project-plan.md) for the phased roadmap.

## What this version does (v0.2.2 — web UI + logs + MQTT/HA + servo test + blind calibration)

On-device WiFi setup, advertises `shutter-hub.local` over mDNS, and serves a
**single-page web UI from LittleFS** (sidebar: Info · MQTT · Servo test · Shutters ·
System · OTA · Logs) over a JSON/REST API. A **live log stream** runs over WebSocket
(`/ws/logs`); **MQTT** connects to a broker and publishes **Home Assistant discovery**
so the hub appears in HA. Custom **OTA** flashes firmware and/or the LittleFS image. The
**Servo test** page is a low-level bench diagnostic (one servo direct from a GPIO, in
degrees) with a persisted **speed slider** (5–120 °/s, default 25). The **Shutters**
page (Phase 2) is per-blind **calibration**: define a shutter, then use a microsecond
scrubber + transport controls (slow-run → stop → frame-step nudge) to snapshot its
closed/open endpoints and Daylight/Privacy favourites — all persisted in NVS (survives a
filesystem OTA). No servo/PCA9685/power hardware
required — it runs on a bare ESP32 dev board. This is the base every later phase
(shutter covers, HomeKit, solar) builds on.

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
later, use **System → WiFi** (scan/connect in place); **Reset WiFi** in the System
page's Quick Actions reboots back into this setup portal.

## Build

`esp32dev` (ESP32-D) is the only build target right now. An `esp32-c3-devkitm-1`
(ESP32-C3) env is defined but **deferred** — excluded from `pio run` until it's
brought into a later release.

```
pio run                            # builds ESP32-D only (default_envs)
pio run -e esp32-c3-devkitm-1      # ESP32-C3 (deferred; build explicitly if needed)
```

Prebuilt bins are collected in [`dist/`](dist/). From **v0.2.0** a release ships
**three bins per board** (the web UI now lives in `data/`):

| Per board | File | Use | Since |
| --------- | ---- | --- | ----- |
| Full image | `shutter-hub-<board>-full-vX.Y.Z.bin` | first USB flash, merged at `0x0` | now |
| Firmware  | `shutter-hub-<board>-ota-vX.Y.Z.bin` | OTA page → **Upload Firmware** | now |
| Filesystem | `shutter-hub-<board>-littlefs-vX.Y.Z.bin` | OTA page → **Upload LittleFS** | **v0.2.0** |

Build the filesystem image with `pio run -e esp32dev -t buildfs` →
`.pio/build/esp32dev/littlefs.bin`. **Flash it alongside the firmware** — without it
the device serves an embedded recovery page (OTA upload only).

_ESP32-C3 bins are not built or released yet._

## First flash (USB — one time only)

After this, every update goes over WiFi (see OTA below).

**Option A — PlatformIO, board on USB:**
```
pio run -e esp32dev -t upload      # auto-detects the port; monitor with:  pio device monitor
```

**Option B — single merged image (NodeMCU-PyFlasher / esptool):**
Flash the `-full-` image at offset `0x0`:
```
esptool --chip esp32 write_flash 0x0 dist/shutter-hub-esp32d-full-v0.0.2.bin
```
In NodeMCU-PyFlasher: select the `...-full-...bin`, address `0x0`, flash.

> Merge recipe (how the `-full-` image is made): `esptool --chip esp32 merge-bin`
> with bootloader + partitions + boot_app0 + firmware. The ESP32-D bootloader sits at
> `0x1000` and the app at `0x10000`.

## Update over the air (every time after the first)

The **OTA Update** page at `http://shutter-hub.local/` is a custom updater: it shows the
installed version, chip, last flash, and two uploaders — **Firmware** and
**Filesystem (LittleFS)** — with an upload log.

1. Build new bins: `pio run` (firmware) and `pio run -t buildfs` (filesystem).
2. **Upload Firmware** with the app bin; **Upload LittleFS** when the web UI (`data/`)
   changed. Upload the filesystem first if both changed, then the firmware (which reboots).
3. The board reboots into the new build — your saved WiFi and settings are untouched
   (they live in NVS, not the app/filesystem partitions).

## Layout

```
firmware/
├─ platformio.ini              board targets + libraries + build flags
├─ include/                    module headers (*.h)
├─ data/                       LittleFS web UI (index.html, style.css, app.js)
├─ src/
│  ├─ main.cpp                 thin entry point — wires the modules together
│  ├─ AppConfig.cpp            persisted settings: device, servo, MQTT, auth (NVS) [real]
│  ├─ Diagnostics.cpp          logging + log ring buffer + WS sink, /info      [real]
│  ├─ WiFiSetup.cpp            WiFiManager AP + captive portal                 [real]
│  ├─ WebUI.cpp                static SPA + JSON API + /ws/logs + mDNS         [real]
│  ├─ Ota.cpp                  custom firmware + LittleFS OTA                  [real]
│  ├─ ServoController.cpp      single-servo µs driver → PCA9685        [Phase 1 real]
│  ├─ Shutters.cpp             per-blind definitions + calibration (NVS) [Phase 2 real]
│  ├─ Mqtt.cpp                 broker connect + HA discovery scaffold  [v0.2.0; covers Phase 4]
│  ├─ HomeKit.cpp              HomeSpan bridge            [stub, Phase 5]
│  └─ LightSensor.cpp          VEML7700 solar protection  [stub, Phase 6]
├─ dist/                       prebuilt bins (gitignored; attached to releases)
└─ README.md
```

The web UI is static files in `data/` (LittleFS), served by `WebUI` over a JSON API +
WebSocket; the firmware falls back to an embedded recovery page if the FS image is
missing (see ADR 0004/0005). `WebUI` is named that (not `WebServer`) to avoid clashing
with the Arduino core's `WebServer.h` that WiFiManager includes.
