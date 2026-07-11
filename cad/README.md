# CAD — 3D-printed parts

Enclosures and mechanical parts for the hub and the per-shutter actuator modules.

## Contents (as produced)

| Path | Part | Notes |
| ---- | ---- | ----- |
| `hub-enclosure/` | Wall-mounted hub box | Compartments for AITRIP trigger, XL4015 (vent slots above), ESP32-D, PCA9685; servo-cable exits; VEML7700 light window |
| `actuator-module/` | P1 — servo cradle bracket | MG90D pocket 23.1×12.5, M2 tab bosses @27.5 mm, ~3 mm base with slotted holes (±4 mm x/y); screws flat to the bottom rail's rear face (hidden, recommended) or room face |
| `slat-hook/` | P2 — slat-hook crank | Clips over the bottom slat ≥10 mm inboard of the tilt rod; M2 ball-stud hole at 26 mm radius, phase −10° (rear) / −170° (front); spare holes 22 / 30 mm |
| `servo-arm/` | P3 — 22 mm servo arm (optional) | Only for the 34 mm-crank torque variant; stock 20 mm horn is the baseline |
| `servo-test-bracket/` | Phase 0 proving rig | Temporary mount to run the force test |

Each part folder holds **editable source** (FreeCAD `.FCStd` / Fusion `.f3d` / OpenSCAD `.scad`)
**and** exported **`.stl`** (print-ready) plus **`.step`** (interchange). Keep source + exports
together so the parts stay editable.

## Key dimensions (see `../docs/tilt-linkage-design.html` — interactive design doc)

- Servo: MG90D case 22.8 × 12.2 × 28.5 mm; tabs 32.5 mm span, Ø2.2 holes @ 27.5 mm centres;
  shaft axis ≈6 mm from the front face; 21T Ø4.8 spline. Verify with calipers — clones vary ±0.3 mm.
- Linkage (corrected four-bar): stock 20 mm horn + slat-hook crank (26 mm pin) + M2 ball-link
  pushrod at **58 mm hole-to-hole** (rear mount; 52 mm for front mount). ~122° servo sweep → 90°
  slat travel, jam margin ≥10 mm, torque ratio ≤0.86.
- Servo axis (rear mount): 23 mm behind the slat-pivot plane (½ frame + ½ servo + 3 mm plate),
  47 mm below the bottom slat pivot — cradle flat on the rail's rear face, fully hidden.
- Shutter geometry: frame/rail thickness **28 mm (measured)**; still to measure: rocker radius
  R ≈ 36 mm, slat pitch ≈ 63 mm, rail top ≈ 45 mm below the bottom pivot, rear depth ≥ 43 mm.

The earlier sketch [`../docs/diagrams/linkage-geometry.svg`](../docs/diagrams/linkage-geometry.svg)
shows the superseded twist-rod concept; the HTML design doc is authoritative.

> Status: **to be produced.** Phase 0 prints the servo-test-bracket + shutter-arm first; final
> enclosures come in Phase 7.
