# Architecture

Design principles, key trade-offs, and topology. Companion to
[project-brief.md](project-brief.md).

## Design principles

- **Hub-centric, not per-shutter controllers.** One ESP32 hub drives many dumb servo actuators.
  Fewer parts, one OTA target, one web UI, genuine synchronisation. (ADR 0001.)
- **Local first.** Full function without cloud: local WiFi, local web UI, local HA, local HomeKit.
- **Configuration over programming.** Shutter count, names, calibration, favourites, thresholds,
  WiFi, MQTT, OTA are all data. Firmware iterates a `shutters[]` array — no code change to add or
  remove a shutter.
- **Commercial-style UX.** Users see Open/Close/Position %/Daylight/Privacy — never servo angles
  or µs values.
- **Precision tilt actuator, not a blind motor.** The job is repeatable slat rotation of a
  lightweight assembly, quietly, in a compact package — not lifting weight.

## Key trade-offs

| Choice | Alternative | Why chosen | ADR |
| ------ | ----------- | ---------- | --- |
| Central hub | 4 independent ESP32 nodes | Shutters share a room; short loom; 4× fewer parts; true sync | 0001 |
| MG90D | MG90S / DS3235 | Digital accuracy; huge torque margin for lightweight slats; DS3235 oversized | 0002 |
| XL4015 5.1 V | LM2596 | Servo peak 2–3 A vs LM2596's realistic ~2 A; XL4015 gives margin | 0003 |
| PCA9685 driver | Direct ESP32 PWM | Clean multi-servo drive, shared I2C with sensor, easy channel expansion | 0003 |
| Custom firmware (HomeSpan+MQTT) | ESPHome | Keeps native HomeKit + custom smooth motion + calibration UI + OTA | 0004 |
| Pushrod + ball link | Direct servo-to-rod coupling | Tolerates misalignment; force off the gear train; easy install/removal | — |

## Hardware topology

```
USB-C PD → AITRIP trigger (12V) → XL4015 (5.1V) ─┬─ ESP32-D VIN
                                                 ├─ PCA9685 V+
                                                 └─ servo rail (+ 1000–2200µF) → MG90D ×N
ESP32-D I2C (GPIO21/22) ── PCA9685 (0x40) ── CH0..CHn → servos
                        └─ VEML7700 (0x10)
Common ground single-point.
```

## Data sources / integration surfaces

- **HomeKit:** HomeSpan bridge, one Window Covering accessory per shutter.
- **Home Assistant:** MQTT discovery, one `cover.*` per shutter + lux/position sensors.
- **Web:** ESPAsyncWebServer, single multi-shutter page at `shutter-hub.local` (status / control /
  calibration / OTA).
- **Config store:** LittleFS JSON (config + web files) + Preferences (calibration values).

## Known gotchas

- **Servo inrush.** All 4 MG90D moving together spike current; stagger start-up in firmware, keep
  the bulk cap, keep ESP32 on the buck rail (not through the board), vent the XL4015.
- **Set XL4015 to 5.1 V before connecting anything** — measure with a meter first.
- **I2C sharing** — PCA9685 (0x40) and VEML7700 (0x10) coexist on one bus; no address clash.
- **Smooth motion** — use stepped `moveTo`, never instant `write`, to protect linkage and gears.
- **Calibration is per-shutter** — panels differ; never assume one set of µs limits fits all.
