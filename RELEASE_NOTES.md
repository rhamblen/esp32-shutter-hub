# v0.4.2 — Home Assistant operating card

A custom **Lovelace card** for the shutters. Frontend only — **no firmware change**, so this
release ships **no new firmware/filesystem bins** (keep running v0.4.1 firmware).

## What's new

**`shutter-hub-card`** — a single tile that shows 1–6 named shutters side by side, each with a
slat glyph that tracks its position. It gives the four blind states — **Open · Close · Daylight ·
Privacy** — plus a **manual position slider** (any 0–100 %) and a **Stop**, and you can drive
**all shutters at once** or **tap one tile** to control just that shutter (*Select all* returns to
the group).

- Binds to the existing MQTT-discovery entities — `cover.<hub>_<id>` and
  `button.<hub>_<id>_daylight` / `_privacy` (button ids auto-derived from the cover).
- Plain custom element, no build step; themes off Home Assistant's own CSS variables.
- Deployed to the **My Home › Shutters** dashboard as an inline module resource.

New [ADR-0007](docs/decisions/0007-ha-lovelace-card.md) and build spec
[docs/ha-lovelace-card.md](docs/ha-lovelace-card.md).

## Deferred (future phase)

The **calibration / config card** is intentionally not included — raw-µs set-and-go, the
full-open/close travel endpoints, and *saving* into the four preset slots need firmware commands
that don't exist yet (`goto_us`, raw-µs readback, `save:open` / `save:close`). It's specified in
[docs/ha-lovelace-card.md](docs/ha-lovelace-card.md) §4 for a later build. Until then, calibrate in
the hub's web UI; this card operates the shutters Home Assistant already knows how to drive.

## Install

Register `ha-card/shutter-hub-card.js` as a Lovelace resource (type *JavaScript Module*), then add
the card to a dashboard:

```yaml
type: custom:shutter-hub-card
title: Shutters
shutters:
  - entity: cover.shutter_hub_shutter_1
  - entity: cover.shutter_hub_shutter_2
```

Full history in [CHANGELOG.md](CHANGELOG.md).
