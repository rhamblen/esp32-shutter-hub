# v0.1.0 — Servo bench test (Phase 1)

First hardware-phase release. On top of the v0.0.3 skeleton, a new **Servo test** tab drives one
servo directly from an ESP32 GPIO so you can exercise the hardware before the PCA9685 / power chain
exist. Still runs on a bare ESP32-D.

## What's new since v0.0.3
- **Servo test tab** — drives **one servo straight from an ESP32 GPIO** (default **GPIO13**,
  changeable and persisted in NVS). Set/validate the signal pin (rejects input-only & flash pins,
  flags strapping pins), a **0–180° angle slider**, **Min / Centre / Max** presets, a non-blocking
  **Sweep** toggle, and **Attach / Detach (release)**. Live status shows pin, state, angle, and pulse
  width. The servo stays **detached at boot** so nothing moves until you act.
- **`ServoController`** promoted from stub to a real single-servo driver (`ESP32Servo`, LEDC-backed;
  50 Hz, 500–2500 µs). REST: `GET /api/servo`, `POST /api/servo/{pin,write,attach,detach,sweep}`.

## Wiring (direct-to-ESP32 bench test)
| Servo wire | Signal | ESP32-D pin |
| ---------- | ------ | ----------- |
| Brown | Ground | any `GND` |
| Red | Power | `5V / VIN` (not 3V3) |
| Yellow | PWM | `GPIO13` (default; changeable in the tab) |

Power the servo from 5 V with a **common ground**. Expect a brief current surge on attach — if the
board resets, power the servo from a separate 5 V supply sharing ground.

## Download (ESP32-D)
| First flash over USB (offset `0x0`) | Update over the air |
| ----------------------------------- | ------------------- |
| `shutter-hub-esp32d-full-v0.1.0.bin` | `shutter-hub-esp32d-ota-v0.1.0.bin` |

_ESP32-C3 bins are deferred to a later release._

## Flash it
First time, over USB:
```
esptool --chip esp32 write_flash 0x0 shutter-hub-esp32d-full-v0.1.0.bin
```
Then join `Shutter-Hub-Setup`, pick your network, and open `http://shutter-hub.local/`. After that,
update over the air from the **Firmware** tab (select the `-ota-` bin, **Flash firmware**). Saved WiFi
and settings live in NVS and survive updates.

## Build from source
PlatformIO project in [`firmware/`](firmware/) — `pio run` builds the ESP32-D target. See
[`firmware/README.md`](firmware/README.md). Full history in [CHANGELOG.md](CHANGELOG.md).
