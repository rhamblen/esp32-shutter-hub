# 0007 — Home Assistant control surface: a custom Lovelace card

- **Status:** Accepted
- **Date:** 2026-07-08

## Context

[0006](0006-ha-integration-mqtt-vs-custom.md) settled the *integration* question —
MQTT Discovery only, no published `custom_components`/HACS **integration**. That
gives Home Assistant native `cover` + `button` + `sensor` entities with zero install
step, but it says nothing about the **dashboard experience**.

The owner wants a single tile that:

- shows **1–6 named plantation shutters side by side**, each with its live slat angle;
- offers **one group control** (Open · Close · Daylight · Privacy) for the whole set;
- lets you **select an individual shutter** and drive just that one;
- keeps **calibration hidden** from everyday use but available as a **separate tile**
  that mirrors the web calibration (scrub position, type an exact value, save into one
  of four slots) — and, on any position change, does a **set-and-go**: the servo
  physically moves to the previewed position so the installer sees the real angle
  before saving.

Stacking built-in HA cards (an `entities` card + four `cover` rows + `button` cards)
can approximate the *function* but not the *form*: no side-by-side slat visual, no
group/individual toggle in one tile, no µs-precise calibration with live preview.

## Decision

**Build a custom Lovelace card (frontend resource), not a stacked built-in layout.**

Two cards ship from one repo/bundle:

1. `shutter-hub-card` — the everyday control tile, with a **group mode** and an
   **individual mode** (see [ha-lovelace-card.md](../ha-lovelace-card.md)).
2. `shutter-hub-calibration-card` — a separate, setup-only tile for calibration with
   **set-and-go** on every position change.

Both are pure frontend: they read/write the MQTT-discovered entities from
[0005](0005-mqtt-command-structure.md). Distribution is a **HACS *frontend*
(dashboard) resource** — a JS bundle registered as a Lovelace resource. This is a
different artifact from the Python **integration** that 0006 ruled out, so the two
decisions are consistent: no Python component to maintain, no config flow, no HA-core
API churn — just a versioned JS file.

## Rationale

- **Form the built-ins can't give** — side-by-side slat glyphs, a single group bar
  that flips to per-shutter scope, and a µs calibration surface are card-level UI, not
  something `entities`/`cover` cards express.
- **Consistent with 0006** — a frontend card is not a custom integration. No
  `custom_components/`, no config flow, no quality-scale treadmill; the card only
  consumes entities HA already has.
- **Config stays browser-owned (0005)** — the card's config is *display* mapping
  (which entity → which name/slot), not device configuration. The hub's web UI remains
  the source of truth for calibration values; the card is a consumer that can *trigger*
  a save, not a second store.
- **Calibration stays out of the way** — a separate card means the everyday tile never
  shows µs sliders, while power users drop the calibration card on an admin dashboard.

## Consequences

- **New firmware surface required for the calibration card.** ADR-0005's command
  vocabulary has no "drive to an absolute pulse width" command and does not publish the
  raw µs value. Set-and-go needs both:
  - a command to move a shutter to an absolute µs (calibration jog), and
  - a state topic publishing the current µs,

  added to the Phase 4 `cmd`/state handlers. Documented as a firmware dependency in
  [ha-lovelace-card.md](../ha-lovelace-card.md); the **default card** needs none of
  this and works against the planned Phase 4 entity set.
- **Shutter count must rise to 6.** `Shutters::MAX` is currently `4`
  (`firmware/include/Shutters.h`). The 1–6 requirement means bumping that constant (and
  re-checking the servo-power budget) so discovery can advertise up to six covers.
- **A second, small codebase** — a JS/TS card bundle with its own build. Accepted: it
  is frontend-only, has no HA-core coupling, and is the only way to get the requested
  form.
- **Not a HACS integration** — this does **not** reopen 0006. If a broker-free path is
  ever wanted, that remains the deferred HTTP-native option in 0006, independent of this
  card.
