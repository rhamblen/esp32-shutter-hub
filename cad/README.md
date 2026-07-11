# CAD — 3D-printed parts

Enclosures and mechanical parts for the hub and the per-shutter actuator modules.

## Contents (as produced)

| Path | Part | Notes |
| ---- | ---- | ----- |
| `hub-enclosure/` | Wall-mounted hub box | Compartments for AITRIP trigger, XL4015 (vent slots above), ESP32-D, PCA9685; servo-cable exits; VEML7700 light window |
| `actuator-module/` | P1 — servo cradle bracket | MG90D pocket 23.1×12.5, M2 tab bosses @27.5 mm, slotted base (±4 mm x/y), sideways stand-off to the rod plane |
| `servo-arm/` | P2 — 25 mm servo arm | Sandwiches the stock horn (2×M2); ball-link holes at 22 / 25 / 28 mm |
| `rod-pin-adapter/` | P3 — tilt-rod pin adapter | Clamps the rod's bottom end (M3 pinch screw); M2 ball-link hole on the staple line (keeps R = 36) |
| `pocket-cover/` | P4 — rail pocket cover | Only if the rail pocket is needed; slot for the pushrod travel fan |
| `servo-test-bracket/` | Phase 0 proving rig | Temporary mount to run the force test |

Each part folder holds **editable source** (FreeCAD `.FCStd` / Fusion `.f3d` / OpenSCAD `.scad`)
**and** exported **`.stl`** (print-ready) plus **`.step`** (interchange). Keep source + exports
together so the parts stay editable.

## Key dimensions (see `../docs/tilt-linkage-design.html` — interactive design doc)

- Servo: MG90D case 22.8 × 12.2 × 28.5 mm; tabs 32.5 mm span, Ø2.2 holes @ 27.5 mm centres;
  shaft axis ≈6 mm from the front face; 21T Ø4.8 spline. Verify with calipers — clones vary ±0.3 mm.
- Linkage (corrected four-bar): 25 mm printed servo arm + M2 ball-link pushrod at **50 mm
  hole-to-hole** (M2×50 cut down ~9 mm per end, or M2×35). ~140° servo sweep → 90° slat travel.
- Servo axis: 8 mm room-side of the slat-pivot plane, 60 mm below the bottom slat pivot —
  usually inside the bottom rail, pocketed ≈39 × 45 × 14 mm from the rail's rear face.
- Shutter geometry (measure before printing): rocker radius R ≈ 36 mm, slat pitch ≈ 63 mm,
  panel thickness ≈ 44 mm, rail top ≈ 45 mm below the bottom pivot.

The earlier sketch [`../docs/diagrams/linkage-geometry.svg`](../docs/diagrams/linkage-geometry.svg)
shows the superseded twist-rod concept; the HTML design doc is authoritative.

> Status: **to be produced.** Phase 0 prints the servo-test-bracket + shutter-arm first; final
> enclosures come in Phase 7.
