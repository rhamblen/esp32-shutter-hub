# v0.3.0 — Build variants + PCA9685 servo backend

The servo hardware is now a **build-time variant** — direct GPIO (one bench actuator) or **PCA9685**
(I2C multi-channel) — and every build says which it is, on-device and in the file name, so an OTA
image is never flashed onto the wrong topology. This release also carries the **Shutters calibration
page** from the 0.2.x dev cycle (see below) for anyone coming from v0.2.1.

## What's new since v0.2.1

### Build variants (board × servo backend)
- Servo driver chosen at compile time: **`-direct`** (ESP32Servo, one servo off a GPIO) or
  **`-pca9685`** (Adafruit PWM driver, PCA9685 over I2C — the production topology).
- Four PlatformIO envs — `esp32d-` / `esp32c3-` × `-direct` / `-pca9685`. Only the ESP32-D pair is
  active; **`esp32d-pca9685` is the default**. C3 variants are defined but deferred.
- Each build carries an **`FW_VARIANT`** id shown on the **Dashboard** and **OTA** screens
  (`GET /api/info` → `variant` + `backend`) and baked into the artifact names.
- New [ADR-0008](docs/decisions/0008-build-variants.md).

### PCA9685 servo backend + adaptive Servo-test page
- On a `-pca9685` build the servo runs through a PCA9685 at `0x40` (50 Hz, µs pulse widths); detach
  releases the channel. The µs position core (slew/speed/sweep) is shared with the direct build.
- The **Servo test** page adapts to the running backend: the direct build keeps its signal-GPIO
  selector; the PCA9685 build gets **I2C SDA/SCL pin selectors** and a **servo-channel (0–15)**
  selector. New REST: `POST /api/servo/channel`, `POST /api/servo/i2c`.

### Also included — Shutters calibration page (from the 0.2.x cycle)
- A dedicated **Shutters** page for per-blind calibration next to the Servo-test diagnostic:
  µs scrubber, transport cluster (slow-run → Stop → frame-step nudge), OPEN/CLOSED endpoints and
  Daylight/Privacy favourites, all persisted in NVS (survives a filesystem OTA). Full detail in
  [CHANGELOG.md](CHANGELOG.md) under 0.2.2.

## Download (ESP32-D)

Per-variant firmware, plus one shared filesystem image (the web UI adapts to the backend at runtime):

| File | Use |
| ---- | --- |
| `shutter-hub-esp32d-pca9685-full-v0.3.0.bin` | First USB flash (PCA9685 build) at offset `0x0` |
| `shutter-hub-esp32d-pca9685-ota-v0.3.0.bin` | OTA → **Upload Firmware** (PCA9685 build) |
| `shutter-hub-esp32d-direct-full-v0.3.0.bin` | First USB flash (direct-GPIO build) at offset `0x0` |
| `shutter-hub-esp32d-direct-ota-v0.3.0.bin` | OTA → **Upload Firmware** (direct-GPIO build) |
| `shutter-hub-esp32d-littlefs-v0.3.0.bin` | OTA → **Upload LittleFS** (shared by both variants) |

_ESP32-C3 bins are deferred to a later release._

## Flash it

First time, over USB (PCA9685 build shown):
```
esptool --chip esp32 write_flash 0x0 shutter-hub-esp32d-pca9685-full-v0.3.0.bin
```
Then join `Shutter-Hub-Setup`, pick your network, and open `http://shutter-hub.local/`.

**Upgrading over the air: flash BOTH images** — the `-ota-` firmware **for your variant** *and* the
shared `-littlefs-` filesystem. Upload the firmware first (auto-reboots), then LittleFS, then reboot,
and **hard-refresh the browser once** (Ctrl+F5) to pick up the new UI cleanly. Confirm the
**Variant** shown on the Dashboard matches the bin you flashed. Saved WiFi, shutters, and settings
live in NVS and survive updates.

## Build from source

PlatformIO project in [`firmware/`](firmware/) — `pio run` builds the default `esp32d-pca9685`,
`pio run -e esp32d-direct` the direct build, and `pio run -e <variant> -t buildfs` the filesystem
image. See [`firmware/README.md`](firmware/README.md). Full history in [CHANGELOG.md](CHANGELOG.md).
