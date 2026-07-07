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

Design complete on paper; **firmware skeleton built** (`v0.0.2`) — a bare ESP32 does on-device
WiFi setup (incl. an in-browser network picker), a tabbed web UI (System / Firmware / Apple Home),
a custom firmware+filesystem OTA updater, and settings/diagnostics in NVS. No servo hardware yet.
See [docs/project-plan.md](docs/project-plan.md) for the phased roadmap and [firmware/](firmware/)
to build/flash. Prebuilt ESP32-D bins ship on each
[release](https://github.com/rhamblen/esp32-shutter-hub/releases).

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

## Licence

MIT (see `LICENSE`).
