# Project Plan — ESP32 Smart Shutter Hub

Phased roadmap. Phases map loosely to minor versions (Phase 1 → v0.1.0). See
[project-brief.md](project-brief.md) for the full spec and [inventory.md](inventory.md) for parts.

## Status

☐ not started · ◐ in progress · ☑ done

| Phase | Version | Title                          | Status |
| ----- | ------- | ------------------------------ | ------ |
| 0     | —       | Mechanical proving / force test| ☐      |
| S     | v0.0.1  | Firmware framework + OTA scaffold | ☑   |
| 1     | v0.1.0  | Bench bring-up (1 servo)       | ☐      |
| 2     | v0.2.0  | Web UI + calibration           | ☐      |
| 3     | v0.3.0  | WiFiManager + mDNS + OTA       | ☐      |
| 4     | v0.4.0  | MQTT / Home Assistant covers   | ☐      |
| 5     | v0.5.0  | HomeKit (HomeSpan bridge)      | ☐      |
| 6     | v0.6.0  | Light sensor + solar logic     | ☐      |
| 7     | v1.0.0  | Enclosures, PCB, all 4 shutters, diagnostics | ☐ |

Documentation-only progress so far: master brief + architecture + ADRs written. No hardware built.

**OTA-first reordering:** the web-server + ElegantOTA stack **and** WiFiManager captive-portal
provisioning (originally inside Phase 3) were pulled forward as a standalone **Phase S** scaffold
(`v0.0.1`, in `firmware/`) so every later phase flashes over WiFi instead of USB, with no
credentials compiled into the binary. Runs on a bare ESP32. (Phase 3 now only needs to fold WiFi
provisioning into the eventual settings UI alongside the servo/HA config.)

---

## Phase S — Firmware framework + OTA scaffold (v0.0.1) ☑
- **Objective:** a buildable base image with on-device WiFi setup + browser control + OTA, before
  any servo work.
- **What we built:** PlatformIO project (`esp32dev` + `esp32-c3-devkitm-1`, Arduino Core);
  WiFiManager AP `Shutter-Hub-Setup` + captive portal storing creds in NVS (survives OTA); mDNS
  `shutter-hub.local`; ESPAsyncWebServer status page with a "Change WiFi" control; ElegantOTA at
  `/update`. Per board: an OTA `firmware.bin` + a merged full-flash image for the one-time USB flash.
- **Exit criteria:** ☑ compiles; set WiFi on-device, then update over the air from the browser.

---

## Phase 0 — Mechanical proving / force test
- **Objective:** confirm the MG90D + M2×50 linkage actually swings a real tilt rod, and fix the
  horn/arm ratio.
- **What we build:** a printed servo test bracket + multi-hole shutter arm; temporary mount on one
  panel.
- **Prerequisites:** MG90D, M2×50 pushrod, horns, a luggage/fishing scale.
- **Do:** measure tilt-rod force through full travel (pull with the scale). `<500 g` → tiny servo
  fine; `500 g–1 kg` → MG90D ideal; `>1 kg` → longer arm / better linkage. Measure closed→open
  rod travel. Choose horn (8/10/12) + arm hole (15/20/25).
- **Exit criteria:** one shutter fully closes and fully opens from servo motion alone, repeatably.

## Phase 1 — Bench bring-up (v0.1.0)
- **Objective:** ESP32-D + PCA9685 + one MG90D moving under serial control.
- **What we build:** breadboard rig on the XL4015 rail (5.1 V) + bulk cap; `ServoController` skeleton.
- **Prerequisites:** Phase 0 geometry; XL4015 set to 5.1 V; PCA9685 wired on I2C (GPIO21/22).
- **Do:** serial commands `open` / `close` / `50`; verify smooth stepped `moveTo`, no ESP32 resets
  under servo load.
- **Exit criteria:** repeatable position control of one servo, stable power.

## Phase 2 — Web UI + calibration (v0.2.0)
- **Objective:** browser control + per-shutter calibration + persisted config.
- **What we build:** ESPAsyncWebServer, LittleFS web files, jog/SET CLOSED/OPEN/favourites,
  JSON config in LittleFS + Preferences.
- **Exit criteria:** calibrate closed/open/favourites in the browser; survive reboot.

## Phase 3 — WiFiManager + mDNS + OTA (v0.3.0)
- **Objective:** field-friendly networking and updates.
- **What we build:** WiFiManager captive portal, `shutter-hub.local`, ElegantOTA `/update`.
- **Exit criteria:** first-boot AP config works; firmware updates over the browser.

## Phase 4 — MQTT / Home Assistant (v0.4.0)
- **Objective:** HA cover entities per shutter.
- **What we build:** MQTT discovery, cover command/state topics, position sensors.
- **Exit criteria:** `cover.front_room_left` etc. appear and control the servo from HA.

## Phase 5 — HomeKit (v0.5.0)
- **Objective:** native Apple Home control.
- **What we build:** HomeSpan bridge exposing one Window Covering per shutter; Siri.
- **Exit criteria:** shutters controllable from the Home app and Siri, alongside HA.

## Phase 6 — Light sensor + solar logic (v0.6.0)
- **Objective:** automatic heat protection.
- **What we build:** VEML7700 driver on shared I2C; trip/clear state machine; lux sensor to HA;
  manual-override 2 h suspend.
- **Exit criteria:** simulated high lux moves shutters to Privacy and returns to Daylight per timers.

## Phase 7 — Production (v1.0.0)
- **Objective:** permanent install.
- **What we build:** hub enclosure (vented over XL4015, light window), per-shutter actuator
  enclosures, PCB/wiring loom, all 4 channels, watchdog/servo-timeout/brownout diagnostics.
- **Exit criteria:** four shutters running reliably behind the wall, OTA-updatable, "build once and
  leave running".

---

## Open decisions

| # | Decision | Notes |
| - | -------- | ----- |
| D1 | Final horn/arm ratio | Resolved by Phase 0 force test; start 10 mm horn + 20 mm arm. |
| D2 | Servo connectors in hub | Grommet holes + strain relief (prototype) vs JST-XH (tidy/serviceable). |
| D3 | Auto-OTA vs web-OTA only | Web OTA (ElegantOTA) certain; `latest.json` auto-update optional. |
| D4 | Spare LM2596 | Superseded by XL4015; keep as spare or drop from BOM. |
| D5 | Voltage monitoring | Whether to populate GPIO34 servo-rail ADC for brownout telemetry. |
| D6 | Config editing UX | Add shutters via web form vs edit JSON directly. |
