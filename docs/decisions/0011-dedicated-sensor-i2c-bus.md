# 0011 — Dedicated I²C bus (Wire1) for the light sensor

- **Status:** Accepted
- **Date:** 2026-07-10
- **Supersedes:** the "one shared I²C bus" note in [architecture.md](../architecture.md) and
  `docs/ai-context.md` ("PCA9685 0x40 + VEML7700 0x10 share one bus, no clash").

## Context

Every document written before Phase 6 assumed the **VEML7700** light sensor would hang off the
**same** I²C bus as the PCA9685 servo driver (GPIO21 SDA / GPIO22 SCL). Electrically that is
perfectly legal — I²C is a multi-drop bus and the two chips have distinct addresses (`0x10` vs
`0x40`), so there is no address clash and no extra pins are needed.

When Phase 6 began, the question was re-opened. The observation that "something is already on
pins 21 and 22" is not in itself a blocker (that is what an addressed bus is *for*), so the real
choice was:

1. **Shared bus** — wire the sensor in parallel with the PCA9685 on `Wire`.
2. **Dedicated bus** — bring up `Wire1` on a second GPIO pair, pins configurable in the web UI.
3. **Analog sensor** — an LDR on one ADC pin instead of the VEML7700.

Two facts decided it. First, the sensor does **not** live next to the PCA9685: it must sit at the
enclosure's light window facing the room, so it is on a pigtail either way — a second bus costs
nothing in placement terms. Second, the hub is destined for a *build-once-and-leave-in-the-wall*
install (Phase 7), where the failure mode that matters is a damaged sensor lead shorting SDA/SCL
and taking the **servo driver** down with it — the shutters would stop responding because a
light sensor died.

The owner confirmed the extra wiring is a non-issue, which removes the shared bus's only real
advantage.

## Decision

**Dedicated bus (option 2).** The VEML7700 runs on **`Wire1`**, on a GPIO pair stored in NVS and
editable from the **Solar** page — defaulting to **SDA 25 / SCL 26**. The PCA9685 keeps `Wire` on
21/22, untouched.

`LightSensor` is a lean **vendored register driver** (no Adafruit library): fixed gain 1/8 +
25 ms integration → ~1.8432 lx/count, full scale ≈ 120 k lx. Reported lux is the **linear**
`counts × resolution`; the Vishay 4th-order high-lux correction is deliberately **not** applied.

Option 3 (analog LDR) is kept alive only as a reserved `type` field in the config — it would need
an ADC pin selector and is not built.

## Rationale

- **Fault isolation is the whole point.** A shorted or chafed sensor lead can wedge its bus; on
  a dedicated bus that costs you the light reading, not the shutters. Two GPIOs are cheap on an
  ESP32-D and the pin budget is nowhere near tight.
- **It works in both build variants.** `Wire.begin()` is only called inside `#if USE_PCA9685`
  (`ServoController.cpp`), so on a `-direct` build there *is* no shared bus to join. `Wire1` is
  uniform across both variants.
- **Independent pull-ups.** Two breakout boards on one bus put their pull-ups in parallel
  (~10k ∥ ~10k ≈ 5k); separate buses sidestep the question entirely.
- **A second bus buys isolation, not reach.** Cable capacitance is the same problem on either
  bus, so this decision does *not* license a long sensor lead (see Consequences).
- **No Adafruit library** because the HomeSpan build already sits at ~90% flash; the library
  drags in Adafruit BusIO + Unified Sensor. A one-screen driver avoids the risk outright
  (measured: 90.6% after the driver landed, 91.2% with the MQTT entities).
- **No lux correction** because the vendor polynomial diverges catastrophically across the
  60–120 k lx band this sensor exists to watch (at 120 k lx it returns ~10⁸). Absolute lux
  accuracy is irrelevant to a threshold the user calibrates against observed readings; a
  **monotonic, stable** signal is what the state machine and the future HA-side calibration need.

## Consequences

- **Reported lux is approximate**, not a calibrated photometric measurement. Documented in the
  UI and in `LightSensor.h`. Thresholds are meaningful relative to what *this* sensor reports.
- **Two more GPIOs are consumed** (default 25/26). GPIO34–39 are rejected in the UI copy: they
  are input-only and cannot drive an open-drain I²C line.
- The sensor lead should still be kept short (ideally < 20 cm) and run as a **twisted
  SDA/SCL/GND trio** with a 0.1 µF decoupling cap at the sensor and its own clean 3V3 feed —
  see [hardware-layout.md](../hardware-layout.md). A JST-XH 4-pin connector keeps it serviceable.
- Pins are changeable at runtime (`LightSensor::reconfigure()` re-inits the bus on save), so a
  wiring change never needs a reflash — only the Solar page.
- If a true photometric reading is ever wanted, swap in the Adafruit library behind the same
  `LightSensor` interface and re-check the flash budget; nothing else changes.
