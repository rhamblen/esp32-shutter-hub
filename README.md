# ESP32 Smart Shutter Hub

A compact, DIY smart-home controller that tilts existing internal plantation shutters with servo
actuators — behaving like a commercial product while staying small, cheap, mains-free (wired 5 V),
and fully local. One wall-mounted **hub** drives a configurable number of shutters (initially four
in the Front Room) and integrates natively with Apple Home and Home Assistant. Replaces a bulky
Zemismart JM36/JC601.

## Features

- **Hub architecture** — one ESP32 drives all shutters via a PCA9685; simple MG90D actuator modules.
- **Variable shutter count** — any number from 1 upward, by configuration only (no firmware change).
- **Apple HomeKit** — HomeSpan bridge exposes each shutter as its own Window Covering accessory.
- **Home Assistant** — one `cover.*` per shutter via MQTT discovery, plus lux/position sensors.
- **Local web UI** — control, per-shutter calibration, and OTA at `http://shutter-hub.local`.
- **Per-shutter calibration** — closed/open limits + named favourites (Privacy, Daylight) in µs.
- **Solar heat protection** — VEML7700 sunlight monitoring with configurable trip/clear thresholds.
- **Manual-override handling** — automation backs off for 2 h after a manual move.

## How it works

![System architecture](docs/diagrams/system-architecture.svg)

One ESP32-D hub speaks to Apple Home (HomeSpan) and Home Assistant (MQTT) over WiFi, and drives the
servos through a PCA9685. A VEML7700 light sensor feeds the solar heat-protection logic. The number
of shutters is configuration, not code.

**Power & wiring**

![Wiring schematic](docs/diagrams/wiring-schematic.svg)

**Actuator linkage**

![Linkage geometry](docs/diagrams/linkage-geometry.svg)

## Hardware (locked)

| Component | Choice |
| --------- | ------ |
| Controller | ESP32-D |
| Servo driver | PCA9685 (16-ch I2C) |
| Servos | MG90D digital metal-gear ×4 |
| Power | USB-C PD → AITRIP trigger (12 V) → XL4015 @ 5.1 V |
| Light sensor | VEML7700 |
| Linkage | M2 × 50 mm ball-link pushrod + printed arms |

## Status

Design complete on paper; **web UI + servo bring-up + blind calibration + build variants** (`v0.3.0`).
The device interface is a **LittleFS single-page app** (sidebar: Info · MQTT · Servo test · Shutters ·
System · OTA · Logs) with a **live log stream** over WebSocket, **MQTT + Home Assistant discovery**
scaffolding, web authentication, and a custom firmware+filesystem OTA updater. From v0.3.0 the servo
backend is a **build variant** — **direct GPIO** (one bench servo) or **PCA9685** (I2C multi-channel) —
identified on the Info/OTA screens and in the artifact names; the **Servo test** page adapts to
whichever it runs (GPIO selector vs I2C-pins + channel selector) with a persisted **speed slider**
(5–120 °/s). The **Shutters** page (Phase 2) is per-blind calibration: name a shutter, then use a
microsecond scrubber + transport controls (slow-run → stop → frame-step nudge) to set its closed/open
endpoints and Daylight/Privacy favourites, all persisted in NVS. Servo positions are remembered across
reboots/OTA so the first move slews instead of snapping.
See [docs/project-plan.md](docs/project-plan.md) for the phased roadmap and [firmware/](firmware/)
to build/flash. Prebuilt ESP32-D bins ship on each
[release](https://github.com/rhamblen/esp32-shutter-hub/releases) — per variant: full (USB) and
firmware (OTA), plus one shared LittleFS filesystem image.

## Repo layout

| Path | Contents |
| ---- | -------- |
| [docs/project-brief.md](docs/project-brief.md) | Master engineering specification |
| [docs/project-plan.md](docs/project-plan.md) | Phased roadmap + status + open decisions |
| [docs/architecture.md](docs/architecture.md) | Principles, trade-offs, topology |
| [docs/inventory.md](docs/inventory.md) | Bill of materials + shutter facts |
| [docs/ai-context.md](docs/ai-context.md) | Cold-start map for the next AI session |
| [docs/decisions/](docs/decisions/) | Architecture Decision Records |
| [docs/diagrams/](docs/diagrams/) | Architecture, wiring, and linkage SVGs |
| [firmware/](firmware/) | ESP32 firmware (PlatformIO, Arduino Core) — build/flash/OTA |
| [hardware/](hardware/) | KiCad schematic / PCB + fabrication outputs |
| [cad/](cad/) | 3D-printer source + STL/STEP for enclosures & parts |
| [CHANGELOG.md](CHANGELOG.md) | Change history (Keep a Changelog + SemVer) |

## License & Legal

### License

This project is licensed under the MIT License — see the [`LICENSE`](LICENSE) file for details.

### Disclaimer

This is a DIY hardware project that drives mains-adjacent mechanical shutters with servos and a
custom power supply. **Use at your own risk** — you are responsible for the electrical safety and
mechanical integrity of anything you build from it. It is provided "as is", without warranty of any
kind. Apple Home integration is via the open-source [HomeSpan](https://github.com/HomeSpan/HomeSpan)
HomeKit library; this project is **not affiliated with, endorsed by, or condoned by Apple Inc.**

### Trademarks

- Apple, HomeKit, and Siri are trademarks of Apple Inc.
- ESP32 is a trademark of Espressif Systems (Shanghai) Co., Ltd.
- Home Assistant is a trademark of the Open Home Foundation.
