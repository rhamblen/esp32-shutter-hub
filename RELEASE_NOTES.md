# v0.6.2 — Check the wiring from the Info page

The hub already knew which bus the PCA9685 sat on, which channel each shutter answered to, and
whether the light sensor had its own I²C lines — but you had to visit three pages to find out. The
**Info** page now lays the whole hardware map out in one table, so what the firmware believes can be
read straight off against the board.

This release also adds a human-readable **brightness percentage** beside the raw lux reading.

## What's new since v0.6.1

### Hardware & wiring table (Info page)

One row per physical device: **which device, on which bus, on which pins, at which address or
channel** — and whether it's actually there.

- **PCA9685 builds** show the driver's I²C bus and whether the VEML7700 is **sharing** it or it's
  **servos only**, its SDA/SCL GPIOs, address `0x40`, and driving-vs-released. Beneath it, an
  indented row per shutter with its **channel** and calibration state.
- **Direct-GPIO builds** show the servo's signal GPIO and say plainly *"no channel — single servo"*.
  No channel rows are drawn, because there is nothing to check.
- **VEML7700** shows dedicated (`Wire1`) or shared with the PCA9685 (`Wire`), the SDA/SCL GPIOs
  **actually in use** — the PCA9685's, when the bus is shared or clamped — address `0x10`, and
  detected / not detected / disabled.

Footnotes call out the cases that bite: a one-controller chip that can't do a dedicated bus, a sensor
that isn't ACKing at `0x10`, and the fact that the servo bus pins are set on the Servo-test page, not
here.

> The table is a **mirror, not a control**. Every value on it is set somewhere else — servo bus pins
> on **Servo test**, channels on **Shutters**, the sensor bus on **Solar**. It exists so all three can
> be double-checked against the board in one place.

### HomeKit status on the Info page

Next to the MQTT broker row: **Disabled** · **Enabled — reboot to start** · **Active — not paired** ·
**Paired — N controller(s)**. The same four states the System ▸ HomeKit tab reports.

### Brightness sensor (0–100 %)

A perceptual companion to the raw lux value, published retained on `<base>/solar/brightness` and
discovered in Home Assistant as **Brightness** (`%`, `mdi:brightness-percent`). Logarithmic —
`20 × log10(clamp(lux, 1, 100000))`, one lux decade per 20 points: a dark room reads 0 %, a lit room
40 %, overcast daylight 80 %, full sun 100 %.

> **Display-only.** The solar state machine still trips on **raw lux**. The log curve deliberately
> compresses the 30–60 k lx band the thresholds live in (clear ≈ 90 %, trip ≈ 96 %), and it isn't
> cleanly invertible — never drive automation from it.

Full detail in [CHANGELOG.md](CHANGELOG.md).

## API

`GET /api/info` gained four objects — `servo`, `sensor`, `homekit` and `shutters`. The sensor pins
reported there are the **active** ones, after any clamp to the shared bus.

## Notes

- **Still not verified against physical VEML7700 hardware** — the sensor isn't wired yet. The table's
  *detected / not detected* row and the brightness curve are exercised through the Solar page's
  simulate-lux slider.
- **The ESP32-C3 is still not shippable and no C3 binaries are attached here.** See
  [docs/pinout.md](docs/pinout.md).

## Download (ESP32-D / WROOM)

Two servo-backend variants; the LittleFS image is shared across both.

| File | Use |
| ---- | --- |
| `shutter-hub-esp32d-pca9685-full-v0.6.2.bin` | First USB flash — PCA9685 (I2C multi-channel) build, at `0x0` |
| `shutter-hub-esp32d-pca9685-ota-v0.6.2.bin`  | Firmware OTA — PCA9685 build |
| `shutter-hub-esp32d-direct-full-v0.6.2.bin`  | First USB flash — direct-GPIO (single bench servo) build, at `0x0` |
| `shutter-hub-esp32d-direct-ota-v0.6.2.bin`   | Firmware OTA — direct-GPIO build |
| `shutter-hub-esp32d-littlefs-v0.6.2.bin`     | Filesystem image (web UI) — shared by both variants |

**Flash the filesystem image too.** The hardware table lives in it; firmware alone leaves you on the
old Info page.

## Flash it

- **USB (first time):** flash the `…-full-…` image for your variant at offset `0x0`, then upload the
  `…-littlefs-…` image via the web UI's OTA page.
- **OTA (update):** on the OTA page, flash the `…-littlefs-…` filesystem first, then the `…-ota-…`
  firmware (which reboots into it).
- WiFi is set on-device via the `Shutter-Hub-Setup` captive portal — the bins carry no credentials.

## Build from source

PlatformIO in [firmware/](firmware/): `pio run -e esp32d-pca9685` (or `-e esp32d-direct`), filesystem
with `-t buildfs`. ESP32-C3 variants compile but are deferred and unshipped.
