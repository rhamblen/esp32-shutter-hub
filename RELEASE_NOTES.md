# v0.4.3 — Card button reorder

A small card-only patch on top of v0.4.2. **No firmware change** — no new firmware/filesystem bins.

## Changed

- **`shutter-hub-card` state buttons** are now ordered **Close · Privacy · Daylight · Open** (was
  Open · Close · Daylight · Privacy) — a natural closed→open progression left to right.

The deployed inline resource on the **My Home › Shutters** dashboard was updated in place; hard-refresh
(Ctrl+F5) to pick it up.

## Note

Nothing else changed. Shutter presets that appear to "do nothing" are a calibration-data issue, not a
card bug: make sure each saved **Daylight**/**Privacy** position sits *between* that shutter's closed
and open endpoints (check `http://shutter-hub.local/api/shutters`).

Full history in [CHANGELOG.md](CHANGELOG.md).
