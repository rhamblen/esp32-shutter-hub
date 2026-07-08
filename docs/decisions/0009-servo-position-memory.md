# 0009 — Servo position memory + assembly "home" convention

- **Status:** Accepted
- **Date:** 2026-07-08

## Context

Hobby servos (MG90D) are **open-loop** — they report no position. The instant a servo receives its
first PWM pulse it drives to the commanded value as fast as it can; before that it's limp. The
firmware's speed limiter only ramps the *commanded* pulse over time, and the servo tracks that number
almost instantly — so a slewed move is smooth **only if the value it starts from is already close to
where the arm physically is.**

On a cold boot the firmware doesn't know any channel's position, so the first move jumps: it commands
some value (previously mid-travel) and the arm slams to it. In the bench/OTA workflow this happens on
*every* reflash; on a real blind a full-speed slam through an unknown arc could strain the linkage.
True zero-jump needs position feedback (an ADS1115 tap on the servo pot, or feedback servos) — real
options, but hardware cost we don't want yet.

## Decision

Make the **assumed** start position match reality, two ways, no hardware:

1. **Persist last-known position to NVS.** `ServoController` records where each drive slot settled
   (direct build: slot 0; PCA9685: slot = channel 0–15) and restores it on boot. Because the hub is
   "build once, leave running" — and the dev loop is OTA reflashing with servos *still powered* — the
   arm hasn't moved across a reboot, so restored == actual and the first move slews smoothly. Writes
   are **debounced** (a settled position is committed ~3 s after the last move, plus immediately on
   detach and on channel switch) to spare the flash during calibration jogging.
2. **Assembly "home" convention.** A slot that has *never* been driven (no NVS value) defaults to
   **`HOME_US` = the minimum-µs endpoint** (arm fully to one side, the "closed" park). The build
   instruction is: **fit the servo horn / linkage with the arm parked at HOME (slat closed).** Then a
   factory-fresh first boot also assumes reality → no jump.

## Consequences

- The jump is gone for the common cases: warm reboot, OTA, normal running, and a fresh install that
  followed the home convention.
- **Residual case:** if a slat is moved by hand while the hub is *unpowered*, the stored/assumed
  position is stale and that one move corrects at full speed toward the target — now a bounded move to
  a known endpoint, not a slam through an unknown arc. Also, a servo can droop slightly under gravity
  when its signal drops on power loss, so a cold-start move may show a small correction. Both are
  documented, not eliminated.
- New NVS keys `svp0..svp15` in the `shutterhub` namespace (`AppConfig::servoPos`/`setServoPos`),
  cleared by a config reset like everything else.
- If true zero-jump after a power-off hand-move is ever required, add **position feedback** — tap the
  servo pots into an **ADS1115** (0x48) on the existing I2C bus and slew from the measured angle, or
  move to feedback/serial-bus servos. Deferred; this ADR is the cheaper software answer.
