# v0.5.4 — Apple HomeKit bridge (Phase 5)

Phase 5 lands the **Apple HomeKit** integration on top of the existing Home Assistant support: each
configured shutter is exposed to the Home app as a **Window Covering**, driven by the same servo
engine as HA/MQTT. This release also carries a run of reliability fixes shaken out on real hardware.

> **HomeKit status — read this.** The bridge is built and runs: it advertises on the network, servos
> and Home Assistant keep working with it enabled, and the pairing code is configurable. **But Apple
> Home device discovery/pairing on the author's own network is still unresolved** — the iPhone
> doesn't find the accessory yet. It's **parked** here as a checkpoint so the rest of the project can
> move on. Everything non-HomeKit is in daily use.

## What's new since v0.4.3

- **Apple HomeKit bridge (HomeSpan)** — one Window Covering accessory per shutter, on HAP port 1201
  alongside the web UI. New **System › HomeKit** tab: enable toggle, bridge name, 8-digit setup code
  (default `748-88-377`) with a random-code generator, live bridge/pairing status, a pairing QR, and
  *Reset pairings*. Uncalibrated shutters are still operable (servo-envelope default). `[v0.5.0]`
- **Reliable reboot** — the web *Reboot* buttons and OTA now restart via a high-priority `esp_timer`
  instead of a main-loop flag that could be starved, so a flashed image actually activates. OTA
  flashing is decoupled from restarting (flash firmware and/or filesystem in any order, then Reboot).
  `[v0.5.1]`
- **HomeKit no longer freezes the servos** — HomeSpan now runs on its own FreeRTOS task; previously
  sharing the main loop, its HAP/pairing work starved servo slewing and MQTT while the bridge ran.
  `[v0.5.3]`
- **Single mDNS owner** — HomeSpan is now the sole Bonjour responder when HomeKit is on (matching the
  proven HomeKey-ESP32 design), so `_hap._tcp` is announced cleanly. `[v0.5.2, v0.5.4]`
- **UI polish** — icons on the System and MQTT sub-tabs; SSL/TLS deliberately skipped (local-LAN
  broker). `[v0.4.4]`

Full detail per version in [CHANGELOG.md](CHANGELOG.md).

## Download (ESP32-D / WROOM)

Two servo-backend variants; the LittleFS image is shared across both.

| File | Use |
| ---- | --- |
| `shutter-hub-esp32d-pca9685-full-v0.5.4.bin` | First USB flash — PCA9685 (I2C multi-channel) build, at `0x0` |
| `shutter-hub-esp32d-pca9685-ota-v0.5.4.bin`  | Firmware OTA — PCA9685 build |
| `shutter-hub-esp32d-direct-full-v0.5.4.bin`  | First USB flash — direct-GPIO (single bench servo) build, at `0x0` |
| `shutter-hub-esp32d-direct-ota-v0.5.4.bin`   | Firmware OTA — direct-GPIO build |
| `shutter-hub-esp32d-littlefs-v0.5.4.bin`     | Filesystem image (web UI) — shared by both variants |

## Flash it

- **USB (first time):** flash the `…-full-…` image for your variant at offset `0x0`, then upload the
  `…-littlefs-…` image via the web UI's OTA page.
- **OTA (update):** on the OTA page, flash the `…-ota-…` firmware and the `…-littlefs-…` filesystem
  (any order), then click **Reboot**.
- WiFi is set on-device via the `Shutter-Hub-Setup` captive portal — the bins carry no credentials.

## Build from source

PlatformIO in [firmware/](firmware/): `pio run -e esp32d-pca9685` (or `-e esp32d-direct`), filesystem
with `-t buildfs`. ESP32-C3 variants are defined but deferred.
