# Changelog

All notable changes to this project are documented here. Format based on
[Keep a Changelog](https://keepachangelog.com); this project uses [SemVer](https://semver.org).
Phases map loosely to minor versions (Phase 1 → v0.1.0).

## [Unreleased]

### Release checklist / notes
- **Three bins per board** once the web UI moves into LittleFS (Phase 2): `full` (USB flash),
  `ota` (firmware), and **`littlefs`** (filesystem image). Build the filesystem image with
  `pio run -e <env> -t buildfs` (→ `.pio/build/<env>/littlefs.bin`) and attach it as
  `shutter-hub-<board>-littlefs-vX.Y.Z.bin`. v0.0.1–v0.1.0 ship only `full` + `ota` (no filesystem
  image yet). See [firmware/README.md](firmware/README.md).

## [0.1.0] — 2026-07-07

First hardware-phase release: single-servo bench test on top of the v0.0.3 skeleton.

### Added
- **Servo test tab (Phase 1)** — a new **Servo test** tab in the device web UI drives **one servo
  directly from an ESP32 GPIO** for hardware bring-up, before the PCA9685 / power chain exist. Signal
  pin defaults to **GPIO13** and is **changeable + persisted** (`AppConfig::servoPin`, NVS). Controls:
  set/validate the signal pin (rejects input-only/flash pins, flags strapping pins), a **0–180° angle
  slider**, **Min / Centre / Max** presets, a non-blocking **Sweep** toggle, and **Attach / Detach
  (release)**. Live status shows pin, attached/sweeping state, angle, and pulse width. The servo stays
  **detached at boot** so nothing moves until you act.
- **`ServoController`** promoted from stub to a real single-servo bench driver
  (`madhephaestus/ESP32Servo`, LEDC-backed; 50 Hz, 500–2500 µs). REST: `GET /api/servo`,
  `POST /api/servo/{pin,write,attach,detach,sweep}`.

### Changed
- `FW_VERSION` bumped to **0.1.0** (Phase 1 servo work).

## [0.0.3] — 2026-07-07

Web-UI refinement on top of v0.0.2.

### Added
- **Dedicated "Change network" page** (`/wifi`) — the WiFi section now has a single **Change network**
  button that opens its own page with the scan → select → password → **Set** flow and a **Back** link,
  instead of doing it inline.
- **Reset** and **Reboot** buttons pinned to the **bottom of the page** (below the tabs), each with a
  confirmation prompt. **Reset** forgets WiFi and restarts into the setup portal; **Reboot** restarts.
- **Three explicit firmware actions** on the Firmware tab — **Flash firmware**, **Flash LittleFS**, and
  **Flash both** (filesystem first, then firmware) — replacing the single auto-detecting button.

### Removed
- **Management access-point toggle** and the inline **Re-run setup** button (the "hub mode" idea) —
  along with the `apEnabled` setting, `/ap` route, `WiFiSetup::setSoftAP()`, and `ap_enabled` in
  `/info`. WiFi is changed in-browser; first-time setup still uses the portal, and **Reset** remains
  as the portal fallback.

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
