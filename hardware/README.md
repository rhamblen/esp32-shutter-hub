# Hardware — schematics & PCB

Circuit design source and fabrication outputs for the Shutter Hub.

## Contents

| File | Purpose |
| ---- | ------- |
| `shutter-hub.kicad_sch` | KiCad schematic — complete ESP32-D circuit (power chain, I2C, servo driver, sensor) |
| `shutter-hub-schematic.svg` | Visual schematic — power chain, loads, wiring, critical build checklist |
| `shutter-hub-breadboard-layout.svg` | KiCad-style breadboard layout (plan view) — component placement, jumper/wire schedule W1–W13, connectors J1–J3, raised modules |

## Status

**Schematic complete** (`shutter-hub.kicad_sch`) for breadboard prototyping. **PCB layout and fabrication** are Phase 7. Currently, the working build is hand-wired on breadboard per Phases 1–6.

## Schematics & Drawings

Human-readable diagrams are in [`../docs/diagrams/`](../docs/diagrams/):

| Diagram | Purpose |
| ------- | ------- |
| [`wiring-schematic.svg`](../docs/diagrams/wiring-schematic.svg) | Power chain & signal routing (USB-C → XL4015 → ESP32-D, PCA9685, VEML7700) |
| [`system-architecture.svg`](../docs/diagrams/system-architecture.svg) | Overall hub architecture and component relationships |
| [`breadboard-plan.svg`](../docs/diagrams/breadboard-plan.svg) | Development breadboard layout (GPIO, I2C, power) |
| [`hub-build-overview.svg`](../docs/diagrams/hub-build-overview.svg) | Physical assembly and enclosure overview |
| [`standoff-mounting.svg`](../docs/diagrams/standoff-mounting.svg) | PCB mounting points and standoff placement |
| [`cable-loom.svg`](../docs/diagrams/cable-loom.svg) | Servo rail and sensor cabling layout |
| [`rail-cuts-bridges.svg`](../docs/diagrams/rail-cuts-bridges.svg) | Servo power rail distribution |

See [`../docs/hardware-layout.md`](../docs/hardware-layout.md) for detailed component placement and [`../docs/pinout.md`](../docs/pinout.md) for GPIO/I2C pin mapping.

## Design intent (see `../docs/project-brief.md` §3–4)

```
USB-C PD → AITRIP trigger (12 V) → XL4015 (5.1 V) ─┬─ ESP32-D VIN
                                                   ├─ PCA9685 V+
                                                   └─ servo rail (+1000–2200 µF) → MG90D ×N
ESP32-D I2C (GPIO21 SDA / GPIO22 SCL) ─┬─ PCA9685 (0x40) → CH0..CHn
                                       └─ VEML7700 (0x10)
Single-point common ground.
```

Design rules to honour on the PCB: keep the servo-power copper wide (2–3 A), place the bulk
capacitor next to the PCA9685 V+, keep the ESP32 rail decoupled from servo current, and leave
airflow / vent area over the XL4015 (see [ADR 0003](../docs/decisions/0003-power-chain-xl4015-pca9685.md)).

> Status: **to be produced.** A hand-wired/protoboard build (Phases 1–6) precedes the PCB
> (Phase 7). Until then this folder holds the wiring diagram only.
