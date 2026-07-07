# CAD — 3D-printed parts

Enclosures and mechanical parts for the hub and the per-shutter actuator modules.

## Contents (as produced)

| Path | Part | Notes |
| ---- | ---- | ----- |
| `hub-enclosure/` | Wall-mounted hub box | Compartments for AITRIP trigger, XL4015 (vent slots above), ESP32-D, PCA9685; servo-cable exits; VEML7700 light window |
| `actuator-module/` | Per-shutter servo bracket | Holds MG90D at the frame edge; ~30×20×15 mm visible footprint |
| `shutter-arm/` | Tilt-rod clamp arm | Multi-hole (15 / 20 / 25 mm) for tuning torque vs. travel |
| `servo-test-bracket/` | Phase 0 proving rig | Temporary mount to run the force test |

Each part folder holds **editable source** (FreeCAD `.FCStd` / Fusion `.f3d` / OpenSCAD `.scad`)
**and** exported **`.stl`** (print-ready) plus **`.step`** (interchange). Keep source + exports
together so the parts stay editable.

## Key dimensions (see `../docs/project-brief.md` §1, §13)

- Servo: MG90D ≈ 23 × 12 × 29 mm.
- Linkage: M2 × 50 mm ball-link pushrod, hole-to-hole 68–78 mm assembled.
- Leverage: servo horn 8/10/12 mm vs. shutter arm 15/20/25 mm (start 10 mm : 20 mm).
- Shutter panel: ~450 mm wide, edge-mounted vertical tilt rod.

A print-orientation / geometry sketch is exported to
[`../docs/diagrams/linkage-geometry.svg`](../docs/diagrams/linkage-geometry.svg).

> Status: **to be produced.** Phase 0 prints the servo-test-bracket + shutter-arm first; final
> enclosures come in Phase 7.
