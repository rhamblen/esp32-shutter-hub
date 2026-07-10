# v0.7.0 — HomeKit pairing works

The bridge finally shows up in Apple Home, pairs, and **syncs both ways** — move a shutter in Home
and it moves in Home Assistant, and vice-versa. Phase 5 has been "built but unpaired" since v0.5.0;
this is the release where it actually works.

The bug was a nasty one, and worth recording. HomeSpan 1.9.1 validates its own mDNS hostname with
`sscanf(hostName, "%m[A-Za-z0-9-]", &d)` — and **newlib-nano, the C library this ESP32 Arduino
toolchain links, doesn't implement the `%m` allocating conversion**. So the check silently failed,
HomeSpan hit its `while(1)` *PROGRAM HALTED* on its background task, and the bridge never advertised
`_hap._tcp` or opened its HAP port. Nothing to do with mDNS ownership, WiFi, or CPU cores — all of
which got chased first. `shutter-hub` was a perfectly valid hostname; the self-check was broken.

> **This is a diagnostic / rollback baseline.** It still carries the investigation's debug logging and
> `[hs]` breadcrumbs. **v0.7.1** strips those and finishes the log-level cleanup — prefer it for daily
> use once it's out. v0.7.0 is here so there's a known-good checkpoint to fall back to.

## What's new since v0.6.2

### HomeKit is discoverable and pairable

- A **vendored patch** (`firmware/patches/HomeSpan.cpp`) replaces HomeSpan's broken `%m` hostname check
  with a nano-safe equivalent. It's re-applied to the library before every build by
  `patches/apply_patches.py`, so it survives a `.pio` wipe and covers all four variants. Details in
  [firmware/patches/README.md](firmware/patches/README.md).
- **mDNS** is now brought up on the main thread (hostname + `_http._tcp`); HomeSpan adds `_hap._tcp`
  on top. `<name>.local` resolves in both HomeKit-on and -off modes.

### A HomeKit health watchdog

If HomeSpan's connect callback hasn't fired within 10 s of boot, the firmware now logs a **WARN** to
the web Logs page — "bridge NOT discoverable, port 1201 closed" — instead of failing silently.

### Log levels, first pass

MQTT publish (`tx …`) traffic dropped to **VERBOSE** (a *failed* publish now logs **WARN**); MQTT
`subscribed` and HomeKit `[dbg]` lines dropped to **DEBUG**. INFO now reads as a clean narrative of
what the hub is doing. The Servo/Solar reclassification finishes in v0.7.1.

Full detail in [CHANGELOG.md](CHANGELOG.md).

## Download

All four variants are attached. The LittleFS image is shared across every variant.

| File | Use |
| ---- | --- |
| `shutter-hub-esp32d-pca9685-full-v0.7.0.bin` | First USB flash — ESP32-D, PCA9685 (I2C multi-channel), at `0x0` |
| `shutter-hub-esp32d-pca9685-ota-v0.7.0.bin`  | Firmware OTA — ESP32-D, PCA9685 |
| `shutter-hub-esp32d-direct-full-v0.7.0.bin`  | First USB flash — ESP32-D, direct-GPIO (single bench servo), at `0x0` |
| `shutter-hub-esp32d-direct-ota-v0.7.0.bin`   | Firmware OTA — ESP32-D, direct-GPIO |
| `shutter-hub-esp32c3-pca9685-full-v0.7.0.bin`| First USB flash — ESP32-C3, PCA9685, at `0x0` *(untested)* |
| `shutter-hub-esp32c3-pca9685-ota-v0.7.0.bin` | Firmware OTA — ESP32-C3, PCA9685 *(untested)* |
| `shutter-hub-esp32c3-direct-full-v0.7.0.bin` | First USB flash — ESP32-C3, direct-GPIO, at `0x0` *(untested)* |
| `shutter-hub-esp32c3-direct-ota-v0.7.0.bin`  | Firmware OTA — ESP32-C3, direct-GPIO *(untested)* |
| `shutter-hub-esp32d-littlefs-v0.7.0.bin`     | Filesystem image (web UI) — shared by all variants |

> **The ESP32-C3 bins are untested engineering builds.** They compile and carry the HomeKit fix, but
> the C3 pinout, servo-GPIO validation and solar bus still need work — see
> [docs/pinout.md](docs/pinout.md). Use the ESP32-D (WROOM) builds for real hardware.

## Flash it

- **USB (first time):** flash the `…-full-…` image for your board at offset `0x0`, then upload the
  `…-littlefs-…` image via the web UI's OTA page.
- **OTA (update):** on the OTA page, flash the `…-littlefs-…` filesystem first, then the `…-ota-…`
  firmware (which reboots into it). This release's web UI is unchanged from v0.6.2, so an OTA update
  only strictly needs the `…-ota-…` firmware.
- **Pairing:** enable HomeKit on the System ▸ HomeKit tab, reboot, then add the accessory in Apple
  Home with the code shown on that tab. WiFi is set on-device via the `Shutter-Hub-Setup` captive
  portal — the bins carry no credentials.

## Build from source

PlatformIO in [firmware/](firmware/): `pio run -e esp32d-pca9685` (or `-e esp32d-direct`), filesystem
with `-t buildfs`. The HomeSpan `%m` patch is applied automatically by `patches/apply_patches.py`.
ESP32-C3 variants compile but are deferred and untested.
