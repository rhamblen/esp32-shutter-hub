# 0010 — Concurrent per-channel servo drive

- **Status:** Accepted
- **Date:** 2026-07-09

## Context

Through v0.3.0 `ServoController` kept **one** slew engine — a single current/target
pulse-width pair — because Phases 1–3 only ever drove one bench servo. On the PCA9685
build, selecting a different channel moved the *focus* of that engine: the old channel
kept holding its last pulse width (the chip generates all 16 waveforms autonomously in
hardware), but an in-flight move on it was abandoned mid-travel.

Phase 4 wires Home Assistant `cover` entities to real shutters. An HA scene like
"close all" publishes one command per cover within milliseconds; under the
single-engine model, each new command would freeze the previous shutter a few percent
into its travel and only the last one commanded would complete.

The alternatives considered:

1. **Per-channel slew state** — every output slews independently and simultaneously.
2. **Queue and serialize** — one move at a time; "close all" becomes a wave taking
   4× the single-shutter travel time, in exchange for ~1-servo peak current.

## Decision

**Per-channel slew state (option 1).** Every physical output — a "**slot**": a PCA9685
channel on that build, the one GPIO servo on a direct build — carries its own
current/target/moving/persistence state. `ServoController::loop()` advances **every**
slot with an outstanding move each 20 ms servo frame; updating a moving channel is one
short I2C write (~0.5–1 ms), so even 16 concurrent moves are a small slice of the frame.

New slot API (used by the MQTT cover handlers): `moveSlotUs`, `stopSlot`, `slotUs`,
`slotTargetUs`, `slotMoving`. The bench/test API (write/jog/run/sweep/attach/detach)
keeps operating on the **active** slot — the test channel picked on the Servo-test
page — and switching focus no longer disturbs an in-flight move on the old channel.
The speed limit (deg/s) stays a single global setting shared by all slots. Per-slot
positions persist to NVS individually (debounced), extending ADR 0009 unchanged.

## Rationale

- **The hardware was sized for this from day one.** ADR 0001 lists *"genuine
  synchronised movement from a single controller"* as a reason the hub topology won,
  and ADR 0001/0003 size the XL4015 rail for **~3–4 A peak with all four servos
  moving at once**. The single-engine model was a bench-phase simplification, not a
  design ceiling.
- **The PCA9685 needs no serialization.** It generates all channels' PWM autonomously;
  the firmware only nudges targets once per frame. The shared I2C bus adds a
  few-millisecond stagger between channel updates — imperceptible against servo travel
  times of hundreds of ms.
- A queue would be throwaway complexity (queue + timeout handling) that undersells the
  hub and would still need replacing later.

## Consequences

- HA scenes and the group bar of the planned Lovelace card (Phase 4b) behave as a user
  expects: all shutters move together.
- **Simultaneous peak current is now real** — ~3–4 A with four MG90Ds moving. Fine on
  the XL4015 rail (ADR 0003); on a **USB-powered bench rig, avoid multi-shutter scenes**
  or expect brownouts.
- Soft-start staggering (offsetting move *starts* by ~200 ms to spread inrush, per the
  ADR 0003 note) is **deferred** — cheap to add on top of per-slot state once real
  four-servo current behaviour is observable on the bench.
- The direct-GPIO build is unaffected (one slot); MQTT commands for any shutter drive
  its single servo.
