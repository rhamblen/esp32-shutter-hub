# Hardware — schematics & PCB

Circuit design source and fabrication outputs for the Shutter Hub.

## Contents (as produced)

| Path | Contents |
| ---- | -------- |
| `shutter-hub.kicad_sch` | KiCad schematic (power chain, ESP32-D, PCA9685, VEML7700, servo outputs) |
| `shutter-hub.kicad_pcb` | KiCad PCB layout (distribution board) |
| `gerbers/` | Fabrication outputs (zipped Gerbers + drill) |
| `bom.csv` | Board bill of materials |

Human-readable diagrams (for the docs/README) are exported as SVG to
[`../docs/diagrams/`](../docs/diagrams/) — start with
[`wiring-schematic.svg`](../docs/diagrams/wiring-schematic.svg).

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
