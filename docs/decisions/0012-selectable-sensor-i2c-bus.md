# 0012 — The sensor I²C bus is a setting, not a constant

- **Status:** Accepted
- **Date:** 2026-07-10
- **Amends:** [ADR 0011](0011-dedicated-sensor-i2c-bus.md) — its reasoning stands unchanged; the
  dedicated bus becomes the **default** rather than the **only** mode.

## Context

ADR 0011 put the VEML7700 on its own `Wire1` bus so a damaged sensor lead could never wedge the
PCA9685 and freeze the servos. That shipped in v0.6.0 and is the right default.

It also, unintentionally, made solar heat protection **impossible on the ESP32-C3**. A dedicated bus
needs a second hardware I²C controller. `SOC_I2C_NUM` is **2** on the ESP32-D and **1** on the C3.
Worse, the Arduino core declares `TwoWire Wire1 = TwoWire(1)` *unconditionally*, so the C3 build
**compiles and links cleanly** — and then `Wire1.begin()` calls `i2cInit(1, …)`, hits
`if (i2c_num >= SOC_I2C_NUM) return ESP_ERR_INVALID_ARG`, and returns false. No crash, no build
error. The user enables the sensor, sees *"not detected"* forever, and goes looking for a wiring
fault that does not exist.

Three ways out were considered:

1. **Disable solar on the C3** — a capability flag plus a "not supported" notice.
2. **Compile-time bus choice** — `Wire1` where available, `Wire` otherwise.
3. **Make the bus a user setting**, defaulting to dedicated, falling back to shared.

Option 2 is not sufficient on its own: the **LittleFS web-UI image is shared byte-for-byte across
every build variant** by design, so the Solar page cannot learn at compile time which bus the
firmware chose. Whatever the firmware decides, the UI must be told at runtime — exactly as the
Servo-test page already adapts to `usesPca`.

## Decision

**Option 3.** `AppConfig::lsBus()` stores a `SensorBus` preference:

| | `BUS_DEDICATED` (default) | `BUS_SHARED` |
| --- | --- | --- |
| Bus | `Wire1` | `Wire` — the PCA9685's |
| Pins | `lsSda`/`lsScl` (25/26), editable | `i2cSda`/`i2cScl` (21/22), **not** separately settable |
| Needs | `SOC_I2C_NUM > 1` | nothing — always available |
| Fault isolation | yes | **no** — a shorted sensor lead can wedge the servos |

`LightSensor::dedicatedSupported()` derives the capability from `SOC_I2C_NUM > 1` — from the SoC
header, not a hand-maintained board list that would rot. A dedicated *preference* on a
one-controller chip is **clamped to shared** and logged, so the feature degrades instead of dying.
`/api/solar` reports `bus`, `dedicatedSupported`, and the *effective* `activeBus`/`activeSda`/
`activeScl`; the Solar page disables the dedicated option and explains why when the chip can't do it.

The driver holds a `TwoWire *` chosen at `begin()`. It always had this shape — it only ever needed a
bus reference.

## Rationale

- **`TwoWire::begin()` is idempotent on an already-started master bus.** The core logs *"Bus already
  started in Master Mode."*, sets `started = true`, and returns without re-initialising or changing
  pins. So shared mode needs no ownership protocol: on a PCA9685 build `ServoController::begin()`
  (which runs first in `main.cpp`) has already brought `Wire` up and our call is a no-op; on a
  `-direct` build nothing had, and our call is the one that initialises it. First caller wins.
- **The C3 gets solar instead of a feature gap.** Option 1 would have shipped a "not supported"
  notice; this ships a working sensor on the only bus that chip has.
- **The default doesn't move.** ESP32-D users get the fault isolation of ADR 0011 without opting in.
  Sharing is a deliberate, documented, opt-in trade.
- **One setting replaces a capability flag, a notice card, and MQTT entity suppression.** Because
  solar now works on every chip, there are no dead HA entities to hide.

## Consequences

- **`reconfigure()` must never `end()` a shared bus.** `TwoWire::end()` genuinely tears the
  peripheral down; calling it on `Wire` would kill the PCA9685 and freeze every servo. The driver
  tracks `g_dedUp` and only ever ends `Wire1`. This is the single sharpest edge in the change.
- **Shared mode re-introduces the coupling ADR 0011 rejected.** Opt-in, off by default, and the UI
  says so plainly. If a sensor lead shorts in shared mode, the shutters stop responding.
- **Shared-mode pins are the PCA9685's pins**, by definition of a shared bus. The Solar page hides
  its pin fields in that mode rather than offering inputs that silently do nothing.
- **Dedicated-bus pins are validated with `ServoController::validGpio()`.** That whitelist is
  ESP32-D-specific — which is *correct* here, because dedicated mode only exists on a
  two-controller SoC, and the only such board we build is the ESP32-D. It closes the v0.6.0 hole
  where GPIO34–39 could be saved as I²C pins.
- **The C3 is still not shippable**, for the *other* reason in [pinout.md](../pinout.md):
  `validGpio()` would accept GPIO25/26/27/32/33 (which don't exist on a C3) and the SPI-flash pins.
  This ADR removes the solar blocker, not that one.
