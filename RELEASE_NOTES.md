# v0.7.1 — HomeKit fix, tidied

A cleanup of [v0.7.0](https://github.com/rhamblen/esp32-shutter-hub/releases/tag/v0.7.0), which is the
release that got **Apple HomeKit pairing working**. Same fix, **no behavioural change** — this just
removes the investigation's debug scaffolding and finishes the log-level cleanup. If you're on v0.7.0,
this is a drop-in firmware OTA.

> New here? The story: HomeSpan 1.9.1 validated its own mDNS hostname with `sscanf("%m…")`, which
> **newlib-nano doesn't implement**, so the bridge silently halted before it ever advertised or opened
> its HAP port. A vendored patch (`firmware/patches/HomeSpan.cpp`) fixes it. Full detail in
> [v0.7.0's notes](https://github.com/rhamblen/esp32-shutter-hub/releases/tag/v0.7.0).

## What's new since v0.7.0

- **Removed** the temporary `[hs]` breadcrumbs from the HomeSpan patch and the `[dbg]` one-shot bridge
  summary — the vendored patch is now the `%m` fix only.
- **Log levels, finished.** Per-action lines (MQTT cover commands + `rx`, servo attach/detach/sweep,
  HomeKit per-move targets, the solar state-machine's "counting…/cancelled" steps) are now **DEBUG**;
  **INFO** is reserved for milestones — boot, connections, config/calibration changes, pairing events,
  the solar *action* taken. MQTT publish traffic stays **VERBOSE** (a failed publish → **WARN**).

**Kept from v0.7.0:** the HomeSpan `%m` patch, main-thread mDNS init, the HomeKit HAP-stall **WARN**
watchdog, and the HomeSpan-state DEBUG log. Full detail in [CHANGELOG.md](CHANGELOG.md).

## Download

All four variants are attached; the LittleFS image is shared across every variant.

| File | Use |
| ---- | --- |
| `shutter-hub-esp32d-pca9685-full-v0.7.1.bin` | First USB flash — ESP32-D, PCA9685 (I2C multi-channel), at `0x0` |
| `shutter-hub-esp32d-pca9685-ota-v0.7.1.bin`  | Firmware OTA — ESP32-D, PCA9685 |
| `shutter-hub-esp32d-direct-full-v0.7.1.bin`  | First USB flash — ESP32-D, direct-GPIO (single bench servo), at `0x0` |
| `shutter-hub-esp32d-direct-ota-v0.7.1.bin`   | Firmware OTA — ESP32-D, direct-GPIO |
| `shutter-hub-esp32c3-pca9685-full-v0.7.1.bin`| First USB flash — ESP32-C3, PCA9685, at `0x0` *(untested)* |
| `shutter-hub-esp32c3-pca9685-ota-v0.7.1.bin` | Firmware OTA — ESP32-C3, PCA9685 *(untested)* |
| `shutter-hub-esp32c3-direct-full-v0.7.1.bin` | First USB flash — ESP32-C3, direct-GPIO, at `0x0` *(untested)* |
| `shutter-hub-esp32c3-direct-ota-v0.7.1.bin`  | Firmware OTA — ESP32-C3, direct-GPIO *(untested)* |
| `shutter-hub-esp32d-littlefs-v0.7.1.bin`     | Filesystem image (web UI) — shared by all variants |

> **The ESP32-C3 bins are untested engineering builds** — they compile and carry the HomeKit fix, but
> the C3 pinout/servo-GPIO/solar bus still need work ([docs/pinout.md](docs/pinout.md)). Use the
> ESP32-D (WROOM) builds for real hardware.

## Flash it

- **From v0.7.0:** the web UI is unchanged, so a firmware-only OTA (`…-ota-…`) is enough.
- **USB (first time):** flash the `…-full-…` image for your board at offset `0x0`, then upload the
  `…-littlefs-…` image via the web UI's OTA page.
- **Pairing:** enable HomeKit on System ▸ HomeKit, reboot, then add the accessory in Apple Home with
  the code shown on that tab. The bins carry no WiFi credentials (`Shutter-Hub-Setup` captive portal).

## Build from source

PlatformIO in [firmware/](firmware/): `pio run -e esp32d-pca9685` (or `-e esp32d-direct`), filesystem
with `-t buildfs`. The HomeSpan `%m` patch is applied automatically by `patches/apply_patches.py`.
ESP32-C3 variants compile but are deferred and untested.
