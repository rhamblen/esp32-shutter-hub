# Changelog

All notable changes to this project are documented here. Format based on
[Keep a Changelog](https://keepachangelog.com); this project uses [SemVer](https://semver.org).
Phases map loosely to minor versions (Phase 1 → v0.1.0).

## [Unreleased]

### Added
- Master engineering brief consolidating all design discussion (`docs/project-brief.md`).
- Phased roadmap with status table and open decisions (`docs/project-plan.md`).
- Architecture notes and trade-off table (`docs/architecture.md`).
- Bill of materials, shutter facts, and power budget (`docs/inventory.md`).
- Cold-start map for future AI sessions (`docs/ai-context.md`).
- ADRs: 0001 hub vs independent nodes, 0002 servo = MG90D, 0003 power chain (XL4015) + PCA9685,
  0004 custom firmware vs ESPHome (with HomeKey-ESP32 reference).
- SVG diagrams: system architecture, power/wiring schematic, actuator linkage geometry
  (`docs/diagrams/`), embedded in the README.
- `hardware/` (KiCad schematic/PCB) and `cad/` (3D-printer source + STL/STEP) folders with READMEs.
- MIT `LICENSE` and `.gitignore`.

### Design decisions (locked)
- **Architecture:** central Shutter Hub + variable MG90D actuator modules (not 4 independent ESP32s).
- **Servo:** MG90D digital metal-gear micro servo.
- **Power:** USB-C PD → AITRIP trigger (12 V) → XL4015 @ 5.1 V (replaces LM2596), 1000–2200 µF cap.
- **Driver:** PCA9685 16-ch I2C, sharing the bus with a VEML7700 light sensor.
- **Linkage:** M2 × 50 mm ball-link pushrod; adjustable horn/arm ratios.
- **Firmware:** custom Arduino (HomeSpan + MQTT), not ESPHome.

_No hardware or firmware built yet — documentation baseline only._
