# 0003 — Power chain (XL4015) and servo driver (PCA9685)

- **Status:** Accepted
- **Date:** 2026-07-07

## Context

The hub must power an ESP32-D plus up to four (later more) MG90D servos from a convenient source.
Servo peak draw is ~2–3 A with all four moving. An LM2596 buck was originally specced; a PCA9685
vs. direct ESP32 PWM choice was also open.

## Decision

1. Power chain: **USB-C PD charger → AITRIP PD trigger (12 V) → XL4015 buck set to 5.1 V**, with a
   **1000–2200 µF** bulk capacitor on the servo rail. **XL4015 replaces the LM2596.**
2. Drive servos through a **PCA9685 16-channel I2C servo driver** (channels 0–3 for the four
   shutters), sharing the I2C bus with the VEML7700.

## Rationale

- LM2596 is realistically ~1.5–2 A (3 A only in bursts, heat-derated) — too close to the 2–3 A
  servo peak. XL4015 (~5 A) leaves ~1 A+ margin → "build once, leave running" reliability.
- PCA9685 gives clean, jitter-free multi-servo PWM, offloads timing from the ESP32, shares one I2C
  bus with the light sensor (0x40 vs 0x10, no clash), and makes adding channels trivial.
- Both parts are ordered / in hand.

## Consequences

- Set XL4015 output to **5.1 V with a meter before connecting anything**.
- Servos powered from the buck rail directly, never through the ESP32 board; common single-point
  ground.
- Enclosure must **vent above the XL4015** (warms under simultaneous servo load).
- Firmware staggers servo start-up to limit inrush; bulk cap smooths dips.
- Spare LM2596 is superseded — keep as spare or drop from BOM (plan D4).
