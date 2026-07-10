# v0.7.2 — Dashboard pairing status + C3 GPIO safety

Two small, safe fixes on top of [v0.7.1](https://github.com/rhamblen/esp32-shutter-hub/releases/tag/v0.7.1).
The web UI is unchanged, so from v0.7.x this is a **firmware-only OTA**.

## What's new since v0.7.1

- **Fixed: the dashboard showed "not paired" when HomeKit *was* paired.** The status came from a
  transient flag that's only set when a pairing *event* fires — so after a reboot or an OTA (pairing
  survives both in NVS) it read false even though Apple Home was still connected. It now reads the
  actual controller list, so it's correct after any restart.
- **ESP32-C3 GPIO safety.** The servo/I²C pin whitelist was the ESP32-D map hard-coded for every chip;
  on a C3 it accepted pins that don't exist and, worse, the **SPI-flash pins (GPIO12–17) whose use
  bricks boot**. It's now chip-aware — the C3 build offers only `{0–10, 18–21}` and refuses the flash
  pins. (The C3 remains an untested engineering build in other respects — see below.)

No behavioural change on the ESP32-D beyond the corrected pairing label. Full detail in
[CHANGELOG.md](CHANGELOG.md).

## Download

All four variants are attached. The LittleFS image is the same for every variant — attached under both
board names so each variant has a matching filename.

| File | Use |
| ---- | --- |
| `shutter-hub-esp32d-pca9685-full-v0.7.2.bin` | First USB flash — ESP32-D, PCA9685 (I2C multi-channel), at `0x0` |
| `shutter-hub-esp32d-pca9685-ota-v0.7.2.bin`  | Firmware OTA — ESP32-D, PCA9685 |
| `shutter-hub-esp32d-direct-full-v0.7.2.bin`  | First USB flash — ESP32-D, direct-GPIO (single bench servo), at `0x0` |
| `shutter-hub-esp32d-direct-ota-v0.7.2.bin`   | Firmware OTA — ESP32-D, direct-GPIO |
| `shutter-hub-esp32c3-pca9685-full-v0.7.2.bin`| First USB flash — ESP32-C3, PCA9685, at `0x0` *(untested)* |
| `shutter-hub-esp32c3-pca9685-ota-v0.7.2.bin` | Firmware OTA — ESP32-C3, PCA9685 *(untested)* |
| `shutter-hub-esp32c3-direct-full-v0.7.2.bin` | First USB flash — ESP32-C3, direct-GPIO, at `0x0` *(untested)* |
| `shutter-hub-esp32c3-direct-ota-v0.7.2.bin`  | Firmware OTA — ESP32-C3, direct-GPIO *(untested)* |
| `shutter-hub-esp32d-littlefs-v0.7.2.bin`     | Filesystem image (web UI) — for the ESP32-D variants |
| `shutter-hub-esp32c3-littlefs-v0.7.2.bin`    | Filesystem image (web UI) — for the ESP32-C3 variants (identical to the ESP32-D image) |

> **The ESP32-C3 bins are untested engineering builds** — they compile and carry the HomeKit fix, and
> the GPIO whitelist can no longer brick the board, but the C3 pinout/servo count/solar bus still need
> real-hardware verification ([docs/pinout.md](docs/pinout.md)). Use the ESP32-D (WROOM) builds for
> real hardware.

## Flash it

- **From v0.7.x:** the web UI is unchanged, so a firmware-only OTA (`…-ota-…`) is enough.
- **USB (first time):** flash the `…-full-…` image for your board at offset `0x0`, then upload the
  `…-littlefs-…` image via the web UI's OTA page.
- **Pairing:** enable HomeKit on System ▸ HomeKit, reboot, then add the accessory in Apple Home with
  the code shown on that tab. The bins carry no WiFi credentials (`Shutter-Hub-Setup` captive portal).

## Build from source

PlatformIO in [firmware/](firmware/): `pio run -e esp32d-pca9685` (or `-e esp32d-direct`), filesystem
with `-t buildfs`. The HomeSpan `%m` patch is applied automatically by `patches/apply_patches.py`.
ESP32-C3 variants compile but are deferred and untested.
