# CAD — 3D-printed parts

Enclosures and mechanical parts for the hub and the per-shutter actuator modules.

## Contents (as produced)

| Path | Part | Notes |
| ---- | ---- | ----- |
| `hub-enclosure/` | Wall-mounted hub box | Compartments for AITRIP trigger, XL4015 (vent slots above), ESP32-D, PCA9685; servo-cable exits; VEML7700 light window |
| `actuator-module/` | P1 — servo cradle (**tape-mount, drawings done; 3MF pending**) | MG90D case bonded in with a 2nd VHB strip (no screws, tabs unused); plate 40×34×2.4 VHB'd to the rail rear face; side walls stop short of the tabs; 4 mm cable gallery + open lay-in slot. Axis (+24.7,−49). Drawings: [servo-cradle-p1.svg](../docs/diagrams/servo-cradle-p1.svg) · design doc §7 |
| `slat-hook/` | P2 — slat-clip crank (**produced**: `slat-clip-crank.3mf`) | Zemismart-style sleeve clip + added drill boss; wedges onto the 8 mm slat, M2 ball stud drilled 12 mm from the rear edge → pin 26.6 mm @ −16°; see [slat-hook/README.md](slat-hook/README.md) |
| `servo-arm/` | P3 — 22 mm servo arm (optional) | Only for the 34 mm-crank torque variant; stock 20 mm horn is the baseline |
| `servo-test-bracket/` | Phase 0 proving rig | Temporary mount to run the force test |

Each part folder holds **editable source** (FreeCAD `.FCStd` / Fusion `.f3d` / OpenSCAD `.scad`)
**and** exported **`.stl`** (print-ready) plus **`.step`** (interchange). Keep source + exports
together so the parts stay editable.

## Key dimensions (see `../docs/tilt-linkage-design.html` — interactive design doc)

- Servo: MG90D case 22.8 × 12.2 × 28.5 mm; tabs 32.5 mm span, Ø2.2 holes @ 27.5 mm centres;
  shaft axis ≈6 mm from the front face; 21T Ø4.8 spline. Verify with calipers — clones vary ±0.3 mm.
- Linkage (v0.9.1 four-bar): stock 20 mm horn + **front-edge** slat hook (pin 24 mm on the
  front-edge line; 06:00 closed → 03:00 open) + **M2×25** ball-link pushrod at **46 mm**
  hole-to-hole. ~114° servo sweep → 90° slat travel, jam margin 11.3 mm, torque ratio ≤0.85.
- **Build baseline (v0.9.2):** the owner's clip drives a **rear pull-down** layout — servo cradle
  tape-mounted on the rail's rear face, axis **(+24.7, −49)**, stock 20 mm horn, M2×35 pushrod at
  58 mm, clip pin 26.6 @ −16°. ~132° sweep, jam margin 9.3 mm, ratio ≤0.77, rear-most 44.7 mm.
  (v0.9.0/0.9.1 variants retained in the design doc as presets.)
- Shutter geometry: frame/rail thickness **28 mm (measured)**; still to measure: rocker radius
  R ≈ 36 mm, slat pitch ≈ 63 mm, rail top ≈ 45 mm below the bottom pivot, rear depth ≥ 43 mm.

[`../docs/diagrams/linkage-geometry.svg`](../docs/diagrams/linkage-geometry.svg) shows the
three-pose geometry (closed / mid / open), generated from the same solver. The HTML design doc is
authoritative and served live at
<https://rhamblen.github.io/esp32-shutter-hub/tilt-linkage-design.html>.

> Status: **to be produced.** Phase 0 prints the servo-test-bracket + shutter-arm first; final
> enclosures come in Phase 7.
