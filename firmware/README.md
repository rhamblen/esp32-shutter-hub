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
later, use **Change network** on the System tab (opens its own `/wifi` page);
**Reset** at the bottom of the page reboots back into this setup portal.

## Build

`esp32dev` (ESP32-D) is the only build target right now. An `esp32-c3-devkitm-1`
(ESP32-C3) env is defined but **deferred** — excluded from `pio run` until it's
brought into a later release.

```
pio run                            # builds ESP32-D only (default_envs)
pio run -e esp32-c3-devkitm-1      # ESP32-C3 (deferred; build explicitly if needed)
```

Prebuilt bins are collected in [`dist/`](dist/). A release ships **three bins per
board** — but the LittleFS filesystem image only exists once the web UI moves into
a `data/` folder (Phase 2), so v0.0.1–v0.0.3 carry just the first two:

| Per board | File | Use | Since |
| --------- | ---- | --- | ----- |
| Full image | `shutter-hub-<board>-full-vX.Y.Z.bin` | first USB flash, merged at `0x0` | now |
| Firmware  | `shutter-hub-<board>-ota-vX.Y.Z.bin` | Firmware tab → **Flash firmware** | now |
| Filesystem | `shutter-hub-<board>-littlefs-vX.Y.Z.bin` | Firmware tab → **Flash LittleFS** | **Phase 2** |

Build the filesystem image (once a `data/` folder exists) with
`pio run -e esp32dev -t buildfs` → `.pio/build/esp32dev/littlefs.bin`.

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

The **Firmware** tab at `http://shutter-hub.local/` is a custom OTA page: it shows the
installed version, the last flash (what/when/result), and two file pickers — a
**Firmware** image and a **Filesystem (LittleFS)** image.

1. Build a new bin (`pio run`), or grab the `-ota-` bin from a release.
2. Select it in the **Firmware image** box (leave Filesystem empty — we don't ship a
   LittleFS image until Phase 2) and click **Flash selected**.
3. The board reboots into the new build — your saved WiFi and settings are untouched
   (they live in NVS, not the app partition).

You can also select both a firmware and a filesystem image to flash together (the
filesystem is written first, then the firmware).

## Layout

```
firmware/
├─ platformio.ini              board targets + libraries + build flags
├─ include/                    module headers (*.h)
├─ src/
│  ├─ main.cpp                 thin entry point — wires the modules together
│  ├─ AppConfig.cpp            persisted settings (NVS)                [real]
│  ├─ Diagnostics.cpp          logging, /info, uptime, reboot          [real]
│  ├─ WiFiSetup.cpp            WiFiManager AP + captive portal         [real]
│  ├─ WebUI.cpp                tabbed status page + routes + mDNS       [real]
│  ├─ Ota.cpp                  ElegantOTA /update                      [real]
│  ├─ ServoController.cpp      PCA9685 + MG90D            [stub, Phase 1/2]
│  ├─ Mqtt.cpp                 HA MQTT discovery          [stub, Phase 4]
│  ├─ HomeKit.cpp              HomeSpan bridge            [stub, Phase 5]
│  └─ LightSensor.cpp          VEML7700 solar protection  [stub, Phase 6]
├─ dist/                       prebuilt bins (gitignored; attached to releases)
└─ README.md
```

The `*.cpp` stubs are empty `begin()` placeholders so each later phase drops into
its own file. A LittleFS `data/` web UI replaces the embedded HTML in Phase 2 (see
ADR 0004). The web module is `WebUI` (not `WebServer`) to avoid clashing with the
Arduino core's `WebServer.h` that WiFiManager includes.
