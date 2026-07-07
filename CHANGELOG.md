# Changelog

All notable changes to this project are documented here. Format based on
[Keep a Changelog](https://keepachangelog.com); this project uses [SemVer](https://semver.org).
Phases map loosely to minor versions (Phase 1 → v0.1.0).

## [Unreleased]

## [0.0.2] — 2026-07-07

Skeleton completion on top of v0.0.1.

### Added
- **Tabbed device web UI** — three tabs: **System** — a **WiFi** section (network, IP, MAC, signal,
  access-point state, in-browser network picker + access-point toggle + Re-run setup) and a
  **System** section (device name,
  firmware version, hostname, uptime, boot count, free heap, last-restart reason, **Restart hub**,
  `/info`); **Firmware** (custom OTA, below); and **Apple Home** (blank placeholder for the Phase 5
  HomeKit UI).
- **Custom OTA page** (replaces ElegantOTA; mirrors HomeKey-ESP32's dual-target updater) — shows the
  installed firmware version, takes a **firmware** image and a **filesystem (LittleFS)** image in
  separate pickers, and flashes either independently or **both together** (filesystem first, then
  firmware, which reboots). Records the **last flash** — target, result, and UTC time (NTP) — shown
  on the page and in `/info`. Uploads stream through the core `Update` library; the ElegantOTA
  dependency is dropped.
- **`AppConfig`** — NVS/Preferences settings store (device name, boot count, AP-enabled flag) that
  survives OTA; the home for future shutter/HomeKit/MQTT settings.
- **`Diagnostics`** — structured serial logging (`LOGI/LOGW/LOGE`), a `/info` JSON endpoint
  (firmware, uptime, heap, reset reason, boot count, `ap_enabled`, WiFi), and a Reboot control.
- **In-browser WiFi change** — the WiFi section scans for nearby networks and lets you pick an SSID
  + password and **switch live, no reboot** (`GET /api/wifi/scan`, `POST /api/wifi/connect`). A wrong
  password reverts to the current network so you're never stranded. A **Re-run setup** fallback
  reboots into the setup portal for moving to a network you can't reach in-browser; first-time
  provisioning still uses the WiFiManager portal. (Replaces the earlier reboot-to-portal "Change
  WiFi" button.)
- **Management access point toggle** — the `Shutter-Hub-Setup` AP is explicitly torn down once WiFi
  is provisioned. A persisted setting (default **off**) plus a WiFi-section button can re-enable it
  as a management AP reachable at `192.168.4.1` — it serves the normal web UI and does **not**
  relaunch the setup portal.
- **Modular firmware layout** — `main.cpp` reduced to a thin entry point; real modules
  `AppConfig` / `Diagnostics` / `WiFiSetup` / `WebUI` / `Ota`, plus empty `begin()` stubs for
  `ServoController` (Phase 1/2), `Mqtt` (Phase 4), `HomeKit` (Phase 5), `LightSensor` (Phase 6).

### Changed
- Captive-portal responsiveness: disable WiFi modem power-save (`WiFi.setSleep(false)`), fixed AP
  channel, weak-AP filtering, and a "scanning takes a few seconds" note on the portal menu.
- Builds are ESP32-D only for now (`default_envs = esp32dev`); the ESP32-C3 target is deferred.

## [0.0.1] — 2026-07-07

First tagged release: documentation baseline + a buildable firmware framework with OTA.

### Added
- **Firmware framework scaffold (v0.0.1)** — first buildable ESP32 firmware (`firmware/`):
  PlatformIO project (Arduino Core) advertising `shutter-hub.local` over mDNS, serving a status
  page, and accepting browser OTA updates at `/update` (ElegantOTA + ESPAsyncWebServer). Runs on a
  bare ESP32 — no servo/PCA9685 hardware.
- **On-device WiFi provisioning** (WiFiManager) — first boot raises a `Shutter-Hub-Setup` access
  point with a captive portal; credentials are stored in NVS and **survive OTA updates**. No WiFi
  credentials are compiled into the binary (mirrors the HomeKey-ESP32 SoftAP + captive-portal
  pattern; pulled forward from Phase 3). A "Change WiFi" control clears saved creds and reopens the
  portal.
- Two board targets: `esp32dev` (ESP32-D, primary/locked board) and `esp32-c3-devkitm-1`
  (ESP32-C3). Each produces a merged full-flash image (one-time USB flash at `0x0`) and an
  app-only OTA bin; all four land in `firmware/dist/`. Releases ship both boards' bins + source.
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

### Notes
- OTA-first reordering: the web-server + ElegantOTA stack **and** the WiFiManager captive-portal
  provisioning were pulled forward from Phase 3 into a standalone "Phase S" scaffold, so every later
  phase flashes over WiFi instead of USB and no credentials ever live in the binary.
- No servo/sensor hardware built yet — the firmware runs on a bare ESP32 dev board.
