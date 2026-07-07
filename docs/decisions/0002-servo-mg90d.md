# 0002 — Servo choice: MG90D

- **Status:** Accepted
- **Date:** 2026-07-07

## Context

The actuator only rotates lightweight shutter slats via a tilt rod (not lifting weight). Expected
force at the rod is well under 1 kg. Candidates considered: tiny 3.7–5 g digital servo, MG90S
(analogue-ish metal gear), MG90D (digital metal gear), DS3235 (35 kg·cm class).

## Decision

Use the **MG90D digital metal-gear micro servo** on all channels.

## Rationale

- Digital control → better positional accuracy and repeatability than analogue micro servos, which
  matters for calibrated favourite positions.
- Metal gears → durability for years of daily cycling.
- ~2 kg·cm (~200 kg·mm) vs. a required ~15–20 kg·mm at a 15–20 mm arm → large safety margin.
- DS3235 is massively oversized/bulky for lightweight slats; a 3.7 g servo *might* work but leaves
  no margin if a panel is stiff.

## Consequences

- Servo rail must handle ~0.8–1 A stall each, ~2–3 A for four moving together (drives the XL4015
  choice — ADR 0003).
- Approx dimensions ~23×12×29 mm set the actuator enclosure size (~30×20×15 mm visible mechanism).
- Final horn/arm ratio confirmed by the Phase 0 force test; start at 10 mm horn + 20 mm arm.
