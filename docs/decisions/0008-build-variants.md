# 0008 — Build variants (board × servo backend)

- **Status:** Accepted
- **Date:** 2026-07-08

## Context

The hub can drive its servos two ways: **directly from an ESP32 GPIO** (one actuator, LEDC PWM via
ESP32Servo — the Phase-1 bench rig) or through a **PCA9685** 16-channel I2C servo driver (the
multi-shutter target, ADR 0003). Both are live hardware paths we want to keep: the direct build is
the simplest way to prove one actuator with nothing but a dev board, and the PCA9685 build is the
production topology. On top of that the project targets two boards — the **ESP32-D** (primary) and a
deferred **ESP32-C3** (different pinout).

That is a 2 × 2 matrix. We need each combination to be **buildable, flashable, and identifiable** —
so an OTA `.bin` on disk, and the running device's info/OTA screens, unambiguously say which servo
hardware the firmware expects. Flashing a PCA9685 image onto a direct-wired board (or vice-versa)
should be obvious, not a silent mismatch.

## Decision

1. The servo backend is a **compile-time choice**, `-D USE_PCA9685=0|1`, selected per PlatformIO env.
   `ServoController` keeps one public API; only its hardware primitives (init, emit-µs,
   attach/detach, addressing) switch on the flag. The µs position core is shared.
2. Every build carries a **`FW_VARIANT`** string (`-D FW_VARIANT=\"esp32d-pca9685\"`) — `board`-`backend`.
   It is exposed at `/api/info` (`variant`, plus a `backend` of `gpio`|`pca9685`) and shown on the
   **Dashboard** and **OTA** screens, and it names the release artifacts:
   `shutter-hub-<variant>-{full,ota,littlefs}-vX.Y.Z.bin`.
3. **Four envs**, one per matrix cell:

   | Env | Board | Backend | Status |
   | --- | ----- | ------- | ------ |
   | `esp32d-direct` | ESP32-D | direct GPIO | active |
   | `esp32d-pca9685` | ESP32-D | PCA9685 | active (default) |
   | `esp32c3-direct` | ESP32-C3 | direct GPIO | deferred |
   | `esp32c3-pca9685` | ESP32-C3 | PCA9685 | deferred |

   `default_envs = esp32d-pca9685` (current focus). Only the ESP32-D envs are built day-to-day; the
   C3 envs are defined but excluded until a release brings them in (its pinout/I2C map differs).
   Releases build all four.
4. The **web UI is a single `data/` tree** — the Servo-test page reads `backend` from `/api/info` and
   renders the right addressing controls at runtime (signal-GPIO selector vs. I2C SDA/SCL + channel
   selector). No per-variant filesystem build; the LittleFS image is identical across variants.

## Rationale

- A compile-time flag (vs. a runtime setting) means the unused servo library isn't even linked, keeps
  the binary honest about what hardware it expects, and avoids a config that could point a build at
  hardware it wasn't built for.
- Naming the variant in the artifact and on-screen removes the single most likely field mistake —
  flashing the wrong servo topology — with zero runtime cost.
- Runtime-adaptive web pages keep one source of truth for the UI; the tiny cost is one `backend`
  branch in the Servo-test JS, far cheaper than a build-time page-assembly pipeline.

## Consequences

- Adding a board or backend is a new env + a `FW_VARIANT` string; no code fork.
- The PCA9685 build needs the I2C bus pins and a test channel — added to `AppConfig` (defaults SDA 21
  / SCL 22 / channel 0 for the ESP32-D) and settable from the Servo-test page.
- Release tooling now produces **up to four × three** bins; day-to-day only the two ESP32-D sets are
  regenerated (the C3 is deferred).
- `ServoController`'s single-servo, one-active-channel model is unchanged — parallel 4-channel drive
  remains a later phase (see [project-plan.md](../project-plan.md)).
