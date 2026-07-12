# P2 — slat-clip crank

**⬇ Download:** [slat-clip-crank.3mf](https://github.com/rhamblen/esp32-shutter-hub/raw/main/cad/slat-hook/slat-clip-crank.3mf)
· [slat-clip-crank.stl](https://github.com/rhamblen/esp32-shutter-hub/raw/main/cad/slat-hook/slat-clip-crank.stl)
(identical geometry; use whichever your slicer prefers)


The Zemismart-style rod-to-slat coupler (`clip1.3mf`) with a **solid drill boss** added under the
bottom strap. Clipped onto the bottom slat it becomes the linkage's crank; an M2 ball-stud through
the boss is the drive pin. Two closed shells in one object — every slicer unions them.

## How the original part works (measured from the mesh)

- **Sleeve**: mouth 8.35 mm, ~67 mm deep, tapering toward a solid nose. An 8 mm slat wedges tight
  where the taper reaches 8.0 (≈12 mm short of the nose) — the taper *is* the retention, so the
  geometry is left untouched.
- **Fin jaw** (5.0 mm gap): factory feature that snaps onto the tilt-rod bar. **Unused here** — the
  clip must sit ≥10 mm inboard of the rod's plane. Leave it or trim it.
- 12 mm wide along the slat; straps 1.35 / 1.75 mm thick.

## Fitting (rear pull-down layout)

1. Push the clip onto the bottom slat **from the rear (window) edge**, ≥10 mm inboard of the
   tilt-rod staple, until it wedges. Nose ends up near the rear edge, fin at the front.
2. Mark the boss **12 mm in from the slat's rear edge**, at the boss's mid-height.
3. Drill Ø1.8 through the boss along the slat direction; fit the M2 ball stud (self-tap), ball on
   the side facing the servo.

Ball centre lands ≈ **26.6 mm radius, −16° phase** from the slat pivot.

## Matching linkage (solved for this pin; slats close rear-edge-up)

| Element | Value |
| ------- | ----- |
| Servo arm | stock **20 mm** horn |
| Pushrod | **M2×35 at 58 mm** hole-to-hole (any owned rod 46–61 works — drop the servo 1 mm per 1 mm of extra rod) |
| Servo axis | **(+21, −49)** from the bottom slat pivot — cradle on the rail's rear face, 1 mm plate |
| Sweep | ~129° of 180° → 90° slat travel, near-linear |
| Jam margin / torque | 9.9 mm / ratio ≤0.79 → ≈2.8 kg·cm at the slats |
| Envelope | x ∈ [+7, +41] — fully behind the panel, nothing on the room side |

Verify against your measurements in the live simulator (crank 26.6 / phase −16):
<https://rhamblen.github.io/esp32-shutter-hub/tilt-linkage-design.html>

## Load-path note

The clip was designed to be dragged by the rod, not to drive the slat. If it slips on the slat
under torque, add two grub-screw dimples through the straps or a drop of silicone in the sleeve.
