# AI Context — cold-start orientation

Dense factual map for the next AI session. Not for end users. Read this first.

## Purpose

Automate 4 existing Front Room plantation shutters (tilt-only) with one **ESP32 Shutter Hub**
driving a **variable** number of **MG90D** servo actuators via a **PCA9685**, integrated with
**Apple HomeKit (HomeSpan)** and **Home Assistant (MQTT)**. Replaces a Zemismart JM36/JC601.

## How to work here

- The **repo is the source of truth for intent + rationale.** Update docs when decisions change.
- Architecture and part choices are **locked via ADRs** in `docs/decisions/` — don't silently
  re-open them; if reversing, write a new ADR.
- Shutter count is **configuration, not code** — never hard-code "4".
- Follow the user's phased-repo doc conventions (status table, ADRs, CHANGELOG per phase).

## File map

| File | Contents |
| ---- | -------- |
| `README.md` | Shop-window overview |
| `docs/project-brief.md` | Master engineering spec (mechanical + electronics + firmware) |
| `docs/project-plan.md` | Phased roadmap + status table + open decisions |
| `docs/architecture.md` | Principles, trade-off table, topology, gotchas |
| `docs/inventory.md` | Shutter facts + BOM + power budget |
| `docs/decisions/000{1..4}` | ADRs: hub, MG90D, XL4015+PCA9685, custom firmware |
| `CHANGELOG.md` | Keep-a-Changelog; update every phase |

## Locked facts

- **Shutters:** 4 panels, ~450 mm wide, 11 slats, ~75 mm slat height, edge tilt rod, lightweight.
- **Servo:** MG90D (digital metal gear, ~2 kg·cm, ~13–14 g, 4.8–6 V).
- **Driver:** PCA9685 (I2C 0x40, CH0–3).
- **Sensor:** VEML7700 (I2C 0x10, solar heat protection).
- **Power:** USB-C PD → AITRIP trigger (12 V) → XL4015 @ **5.1 V** → ESP32 + PCA9685 + servo rail;
  1000–2200 µF bulk cap; common ground. XL4015 replaced LM2596.
- **Linkage:** M2×50 mm ball-link pushrod (hole-to-hole 68–78 mm); horns 8/10/12 mm; printed arm
  holes 15/20/25 mm; start 10 mm horn + 20 mm arm.
- **Pinout:** GPIO21 SDA, GPIO22 SCL, GPIO34 optional servo-rail ADC, GPIO0 boot.
- **Firmware:** custom Arduino (not ESPHome); HomeSpan bridge + MQTT; ESPAsyncWebServer + LittleFS;
  WiFiManager; ElegantOTA; `ServoController.moveTo()` smooth/staggered.
- **Solar:** trip lux>60000/10min → Privacy; clear lux<30000/20min → Daylight.
- **Manual override:** suspend automation 2 h after a manual move, per shutter.

## Build phases

Phase 0 mech force test → 1 bench (1 servo) → 2 web UI+cal → 3 WiFiManager+mDNS+OTA →
4 MQTT/HA → 5 HomeKit → 6 light/solar → 7 production (enclosures, PCB, 4 shutters, diagnostics).
Nothing built yet — docs only.

## Gotchas

- Set XL4015 to 5.1 V before wiring anything.
- Stagger servo start-up; keep bulk cap; ESP32 on the rail, not through the board; vent the XL4015.
- Stepped `moveTo`, never instant `write`.
- Calibration is per-shutter — panels differ.
- I2C: PCA9685 0x40 + VEML7700 0x10 share one bus, no clash.
