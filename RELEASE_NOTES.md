# v0.6.0 — Light sensor + solar heat protection (Phase 6)

Phase 6 teaches the hub to watch the sun. A **VEML7700** ambient-light sensor feeds a trip/clear
state machine: when the light stays above a threshold for a set dwell, every calibrated shutter
moves to a preset you choose; when it stays below a second threshold, a second action runs. The gap
between the two thresholds is hysteresis, so a passing cloud never flaps the blinds.

> **Build it before the sensor arrives.** The Solar page carries a **simulate-lux slider** — drive
> the whole state machine, watch the shutters move, and tune the thresholds with no sensor fitted.
> Everything in this release was exercised that way; **it has not yet been verified against physical
> VEML7700 hardware.**

## What's new since v0.5.4

- **Solar heat protection** — new `LightSensor` (real VEML7700 driver, replacing the stub) and
  `SolarLogic` (`idle → counting-trip → tripped → counting-clear`) with independent dwell timers.
  Defaults: trip above **60 000 lx for 10 min → Privacy**; clear below **30 000 lx for 20 min →
  do nothing**.
- **The sensor gets its own I²C bus.** It runs on **`Wire1`** (default **SDA 25 / SCL 26**, editable
  in the UI), *not* the PCA9685's bus — so a damaged sensor lead can never wedge the servo driver.
  This reverses an earlier design assumption; the reasoning is in
  [ADR 0011](docs/decisions/0011-dedicated-sensor-i2c-bus.md).
- **Five-valued actions.** The bright and clear targets are each **Open · Closed · Daylight ·
  Privacy · Do nothing**. `Do nothing` advances the state but moves nothing — so a trip to Privacy
  isn't automatically undone, and setting *both* to `Do nothing` turns the hub into a pure
  sensor/reporter.
- **Manual override** — a move you make yourself (web recall, or an HA cover/position/jog/recall
  command) suspends automation on **that shutter for 2 hours**. The hub keeps tracking the light.
- **New Solar page** — Status (live lux, state, dwell countdown, simulate slider), Sensor (type,
  `Wire1` pins, live *detected @0x10* pill), Sensitivity (four numbers), Actions (both targets +
  automation switch). Applies **without a reboot**.
- **Home Assistant** — discovery now publishes an **illuminance sensor**, a **solar-state sensor**, a
  **Solar automation switch**, and two **writable `number`** entities for the trip and clear lux
  thresholds. Writing a clear ≥ trip is rejected (inverted hysteresis oscillates).
- **`shutter-hub-card` v0.6.0** — optional solar toggle + live lux caption in the group header, via
  the new `solar_switch` / `solar_lux` / `solar_state` config keys. Omit them and the card is
  unchanged, so installs without a sensor are unaffected.

Full detail per version in [CHANGELOG.md](CHANGELOG.md).

### Notes

- **Reported lux is approximate.** The driver reports a linear `counts × resolution` at a fixed
  sun-range setting (gain 1/8, 25 ms integration, full scale ≈ 120 k lx); the vendor's high-lux
  correction polynomial is deliberately skipped because it diverges across the 60–120 k lx band the
  sensor exists to watch. Thresholds are calibrated against what *this* sensor reports.
- **Flash is now 91.2%** of the app partition on `esp32d-pca9685`. The VEML7700 driver is hand-rolled
  rather than pulled from Adafruit precisely to stay inside that budget.

## Download (ESP32-D / WROOM)

Two servo-backend variants; the LittleFS image is shared across both.

| File | Use |
| ---- | --- |
| `shutter-hub-esp32d-pca9685-full-v0.6.0.bin` | First USB flash — PCA9685 (I2C multi-channel) build, at `0x0` |
| `shutter-hub-esp32d-pca9685-ota-v0.6.0.bin`  | Firmware OTA — PCA9685 build |
| `shutter-hub-esp32d-direct-full-v0.6.0.bin`  | First USB flash — direct-GPIO (single bench servo) build, at `0x0` |
| `shutter-hub-esp32d-direct-ota-v0.6.0.bin`   | Firmware OTA — direct-GPIO build |
| `shutter-hub-esp32d-littlefs-v0.6.0.bin`     | Filesystem image (web UI) — shared by both variants |

**The filesystem image is mandatory this time** — the Solar page lives in it. Flash firmware alone
and you'll get the old UI with no Solar tab.

## Flash it

- **USB (first time):** flash the `…-full-…` image for your variant at offset `0x0`, then upload the
  `…-littlefs-…` image via the web UI's OTA page.
- **OTA (update):** on the OTA page, flash the `…-ota-…` firmware and the `…-littlefs-…` filesystem
  (any order), then click **Reboot**.
- WiFi is set on-device via the `Shutter-Hub-Setup` captive portal — the bins carry no credentials.

## Wiring the sensor

Four wires to the VEML7700 breakout: **3V3 · GND · SDA → GPIO25 · SCL → GPIO26** (change the pins on
the Solar page if you prefer others — but never GPIO34–39, which are input-only and cannot drive
I²C). Keep the lead short, twist SDA/SCL with GND, and put a 0.1 µF cap at the sensor. Details in
[docs/hardware-layout.md](docs/hardware-layout.md).

## Build from source

PlatformIO in [firmware/](firmware/): `pio run -e esp32d-pca9685` (or `-e esp32d-direct`), filesystem
with `-t buildfs`. ESP32-C3 variants are defined but deferred.
