# v0.6.1 — Selectable sensor I²C bus (and solar on the ESP32-C3)

v0.6.0 gave the light sensor its own I²C bus so a damaged sensor lead could never wedge the servo
driver. That's still the right default — but it quietly made solar heat protection **impossible on
the ESP32-C3**, which has only one I²C controller.

The bus is now a **setting**. It defaults to dedicated, and falls back to shared where the chip can't
do better. This release also brings the full installation and user guides, and a pinout reference.

> **Why it failed silently.** The Arduino core declares `Wire1` unconditionally, so the C3 build
> compiled and linked without complaint. Only at runtime does `Wire1.begin()` reach
> `if (i2c_num >= SOC_I2C_NUM) return ESP_ERR_INVALID_ARG` — and the sensor reports *"not detected"*
> forever, indistinguishable from a wiring fault.

## What's new since v0.6.0

- **The sensor's I²C bus is a setting** ([ADR 0012](docs/decisions/0012-selectable-sensor-i2c-bus.md)),
  chosen on the **Solar** page:
  - **Dedicated (default)** — its own `Wire1` on SDA 25 / SCL 26. A sensor-lead fault can't touch the
    servo bus. Needs a chip with two I²C controllers (the ESP32-D has two).
  - **Shared** — rides the PCA9685's `Wire` on GPIO21/22. Saves two pins; always available; the only
    option on a single-controller chip. Gives up the fault isolation.
- **Solar now works on the ESP32-C3.** A dedicated preference is clamped to shared, logged, and the
  Solar page disables the dedicated option with an explanation rather than leaving you to guess.
- **Dedicated-bus pins are validated.** v0.6.0 would happily save GPIO34–39 as I²C pins; they're
  input-only and can never drive an open-drain line. `POST /api/solar` now rejects them, along with
  identical or non-output-capable pins.
- **[docs/installation.md](docs/installation.md)** — end-to-end install guide, each step with its own
  troubleshooting table.
- **[docs/user-guide.md](docs/user-guide.md)** — everyday operation: the four saved positions, the
  four control faces, solar behaviour incl. hysteresis and the 2 h manual override, recalibration.
- **[docs/pinout.md](docs/pinout.md)** — GPIO map per board, the pins the firmware rejects and why,
  and a proposed ESP32-C3 map.

Full detail in [CHANGELOG.md](CHANGELOG.md).

### Notes

- **Shared mode re-couples the sensor and the servos.** A shorted sensor lead can wedge the servo bus.
  It's opt-in, off by default on the ESP32-D, and the UI says so. ADR 0011's reasoning is unchanged —
  it's now the default rather than the only mode.
- **The ESP32-C3 is still not shippable and no C3 binaries are attached here.** This release removes
  the *solar* blocker; `ServoController::validGpio()` still carries the ESP32-D whitelist and would
  accept pins that don't exist on a C3 (25/26/27/32/33) or that are its SPI flash (12–17). Both C3
  envs do compile (89.5 % / 90.1 % flash). See [docs/pinout.md](docs/pinout.md).
- **Still not verified against physical VEML7700 hardware** — the sensor isn't wired yet. Everything
  continues to be exercised through the Solar page's simulate-lux slider.

## Download (ESP32-D / WROOM)

Two servo-backend variants; the LittleFS image is shared across both.

| File | Use |
| ---- | --- |
| `shutter-hub-esp32d-pca9685-full-v0.6.1.bin` | First USB flash — PCA9685 (I2C multi-channel) build, at `0x0` |
| `shutter-hub-esp32d-pca9685-ota-v0.6.1.bin`  | Firmware OTA — PCA9685 build |
| `shutter-hub-esp32d-direct-full-v0.6.1.bin`  | First USB flash — direct-GPIO (single bench servo) build, at `0x0` |
| `shutter-hub-esp32d-direct-ota-v0.6.1.bin`   | Firmware OTA — direct-GPIO build |
| `shutter-hub-esp32d-littlefs-v0.6.1.bin`     | Filesystem image (web UI) — shared by both variants |

**Flash the filesystem image too.** The bus selector lives in it; firmware alone leaves you on the
old Solar page with no way to pick a bus.

## Flash it

- **USB (first time):** flash the `…-full-…` image for your variant at offset `0x0`, then upload the
  `…-littlefs-…` image via the web UI's OTA page.
- **OTA (update):** on the OTA page, flash the `…-ota-…` firmware and the `…-littlefs-…` filesystem
  (any order), then click **Reboot**.
- WiFi is set on-device via the `Shutter-Hub-Setup` captive portal — the bins carry no credentials.

## Wiring the sensor

**Dedicated bus (default):** 3V3 · GND · SDA → GPIO25 · SCL → GPIO26.
**Shared bus:** wire SDA/SCL in parallel with the PCA9685 on GPIO21/22.

Either way keep the lead short, twist SDA/SCL with GND, and put a 0.1 µF cap at the sensor. Never use
GPIO34–39 — input-only, they cannot drive I²C. Details in
[docs/hardware-layout.md](docs/hardware-layout.md) and [docs/pinout.md](docs/pinout.md).

## Build from source

PlatformIO in [firmware/](firmware/): `pio run -e esp32d-pca9685` (or `-e esp32d-direct`), filesystem
with `-t buildfs`. ESP32-C3 variants compile but are deferred and unshipped.
