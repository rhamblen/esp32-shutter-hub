# Project Plan — ESP32 Smart Shutter Hub

Phased roadmap. Phases map loosely to minor versions (Phase 1 → v0.1.0). See
[project-brief.md](project-brief.md) for the full spec and [inventory.md](inventory.md) for parts.

## Status

☐ not started · ◐ in progress · ☑ done

| Phase | Version | Title                          | Status |
| ----- | ------- | ------------------------------ | ------ |
| 0     | —       | Mechanical proving / force test| ☐      |
| S     | v0.0.1–3| Firmware framework + OTA scaffold | ☑   |
| 1     | v0.1.0  | Bench bring-up (1 servo)       | ☑      |
| S2    | v0.2.0  | Web UI shell (LittleFS SPA) + Logs (WS) + MQTT/HA config | ☑ |
| 2     | v0.2.x–v0.3.0 | Shutters config + calibration; PCA9685 backend + build variants | ☑ |
| 4     | v0.4.0  | MQTT / Home Assistant covers   | ☑      |
| 4b    | v0.4.x  | HA Lovelace operating card     | ☑      |
| 5     | v0.5.0  | HomeKit (HomeSpan bridge)      | ☐      |
| 6     | v0.6.0  | Light sensor + solar logic     | ☐      |
| 7     | v1.0.0  | Enclosures, PCB, all 4 shutters, diagnostics | ☐ |
| 8     | —       | HA calibration card (optional) | ☐      |

**Sequence note (post-v0.1.0):** the polished web UI, live logs, and MQTT/HA *config* were pulled
forward — they all run on a bare ESP32 before servo hardware exists. So the old "Phase 2 Web UI" and
"Phase 3 WiFiManager/OTA" are effectively **done** (Phase S + S2); Phase 3 is retired (WiFi
provisioning now lives in the System > WiFi sub-tab). Per-shutter **calibration** (Phase 2) is now
**done** on real PCA9685 hardware (v0.3.0); what remains hardware-coupled is MQTT **cover control**
(Phase 4).

**Build variants (v0.3.0):** the servo backend is now a compile-time choice — **direct GPIO** (one
bench actuator) or **PCA9685** (I2C multi-channel) — with a `FW_VARIANT` id surfaced on the
info/OTA screens and in the artifact names. Four envs (`esp32d-` / `esp32c3-` × `-direct` /
`-pca9685`); only the ESP32-D pair is active, `esp32d-pca9685` is the default. See
[decisions/0008-build-variants.md](decisions/0008-build-variants.md). Servo positions are remembered
across reboots/OTA (NVS) with an assembly "home" default, so the first move slews rather than snaps
([decisions/0009-servo-position-memory.md](decisions/0009-servo-position-memory.md)). From **v0.4.0**
servo drive is **concurrent per channel** — every commanded output slews independently, so an HA
"close all" moves all shutters at once ([decisions/0010-concurrent-servo-drive.md](decisions/0010-concurrent-servo-drive.md)).

**OTA-first reordering:** the web-server stack **and** WiFiManager captive-portal provisioning
(originally Phase 3) were pulled forward as **Phase S** (`v0.0.1`) so every later phase flashes over
WiFi, no credentials in the binary. **Phase S2** (`v0.2.0`) then moved the UI into a LittleFS
single-page app (sidebar shell, WebSocket logs, MQTT/HA discovery config) — see
[decisions/0005-mqtt-command-structure.md](decisions/0005-mqtt-command-structure.md).

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

## Phase S2 — Web UI shell + Logs + MQTT/HA config (v0.2.0) ☑
- **Objective:** a maintainable, polished device UI + observability + MQTT groundwork, all runnable
  on a bare ESP32 before servo hardware.
- **What we built:** LittleFS single-page app (`data/{index.html,style.css,app.js}`, sidebar shell
  modelled on HomeKey-ESP32) served over a JSON/REST API; **live log stream** over `/ws/logs`
  (ring buffer + level filters); **MQTT** broker config + **Home Assistant discovery** for hub
  diagnostic sensors (`PubSubClient`, config in NVS); **System** page with quick actions, WiFi
  sub-tab, and web-auth. Embedded recovery page if the FS image is missing.
- **Exit criteria:** ☑ compiles; UI served from LittleFS; logs stream live; MQTT connects and the
  hub appears in Home Assistant.

## Phase 2 — Shutters config + calibration; PCA9685 backend + build variants (v0.2.x–v0.3.0) ☑
- **Objective:** define shutters and calibrate each per-panel, in the browser, persisted — on real
  PCA9685 hardware.
- **What we built:** a **Shutters** config surface (count, friendly names, PCA9685 channels — manual,
  no auto-scan; see ADR 0005) + jog / SET CLOSED / SET OPEN / **Daylight**/**Privacy** favourites,
  stored in NVS so a filesystem OTA doesn't wipe calibration — a **separate page** from the low-level
  **Servo test** bench diagnostic (the old Actions tab). Then (v0.3.0) the **PCA9685 servo backend**
  went live as a **build variant** (`USE_PCA9685`, [ADR 0008](decisions/0008-build-variants.md)) with
  a runtime-adaptive Servo-test page (I2C-pin + channel selectors); selecting a shutter routes drive
  to its channel, channel-switching causes no motion, and **servo positions persist in NVS**
  ([ADR 0009](decisions/0009-servo-position-memory.md)) so the first move slews from where the arm is.
- **UI mockup:** [diagrams/calibration-page.svg](diagrams/calibration-page.svg) — dark-theme wireframe
  of the Shutters page: shutter selector, µs scrubber, transport cluster (slow-run → stop → nudge),
  endpoint SET buttons, and Daylight/Privacy favourites.
- **Prerequisites:** Phase 1 servo control; PCA9685 on I2C.
- **Exit criteria:** ☑ calibrate closed/open + Daylight/Privacy per shutter in the browser; drive the
  real per-channel servo; survive reboot. (Parallel multi-channel drive followed in v0.4.0, ADR 0010.)

## Phase 3 — WiFiManager + mDNS + OTA — RETIRED (folded into Phase S / S2)
- Captive-portal provisioning + mDNS + custom OTA shipped in Phase S; in-place WiFi change now lives
  in **System > WiFi**. No separate release.

## Phase 4 — MQTT / Home Assistant covers (v0.4.0) ☑
- **Objective:** HA cover entities per shutter, driving real servos.
- **Config done in v0.2.0** (broker connection, HA discovery, ADR-0005 command structure).
- **What we built (v0.4.0):** per-shutter `cover` + six `button` entities via MQTT discovery
  (re-published automatically on any shutter config change, deleted entities cleaned up); the
  `<base>/cover/<id>/{set,position/set,cmd}` handlers wired to each shutter's calibrated
  endpoints/favourites on its own channel; retained `position`/`state` publishing that also tracks
  web-UI moves; **concurrent per-channel drive** ([ADR 0010](decisions/0010-concurrent-servo-drive.md))
  so scenes move all shutters simultaneously; a live per-shutter **Topics** map on the MQTT page.
  Calibration-card commands (`goto_us`, `save:open`/`save:close`, raw-µs readback) stay
  deferred to Phase 8.
- **Exit criteria:** ☑ verified on hardware (2026-07-09, MQTT Explorer + HA) — covers appear and
  control the servo, jog buttons and Daylight/Privacy presets work, state/position topics track
  as expected.

## Phase 4b — Home Assistant Lovelace operating card (v0.4.x) ☑
- **Objective:** a purpose-built everyday dashboard tile for the shutters.
- **What we built:** `shutter-hub-card` — a custom Lovelace **frontend** card (plain custom
  element, registered as a Lovelace resource — **not** a HACS integration, see ADR-0007): 1–6 named
  shutters side by side with a live slat glyph; **group mode** (one Open/Close/Daylight/Privacy bar
  for all) that flips to **individual mode** (slider + buttons scoped to a selected shutter).
- **Spec:** [ha-lovelace-card.md](ha-lovelace-card.md) — entity model, config schema, card layout.
- **Prerequisites:** Phase 4 cover/button discovery.
- **Status (v0.4.2):** shipped — [`ha-card/shutter-hub-card.js`](../ha-card/shutter-hub-card.js),
  deployed to the *My Home › Shutters* dashboard as an inline Lovelace resource: group + per-shutter
  Open/Close/Daylight/Privacy and a manual position slider.
- **Exit criteria:** ☑ control card drives all/one shutter and follows the HA theme.
- The **calibration card** originally bundled in this phase is split out to **Phase 8 (optional)**
  together with the firmware commands it needs.

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

## Phase 8 — HA calibration card (optional)
- **Objective:** a setup-only calibration tile for a hidden/admin dashboard, so a shutter can be
  calibrated from HA without opening the hub web UI. **Optional** — the web UI Shutters page already
  covers calibration in full; build only if the convenience proves worth it.
- **What we build:** `shutter-hub-calibration-card` — shutter selector, µs scrub slider,
  exact-µs entry, and four save slots (Full open/close, Daylight, Privacy). **Set-and-go:** every
  position change drives the servo to that position so the real slat angle is visible before saving.
- **Spec:** [ha-lovelace-card.md](ha-lovelace-card.md) §4 — layout, interactions, and the firmware
  additions it needs.
- **Prerequisites:** firmware calibration transport (spec §4) — a `goto_us:<µs>` command, retained
  raw-µs readback, `save:open` / `save:close`, surfaced via MQTT discovery; plus `Shutters::MAX`
  raised from 4 → 6.
- **Exit criteria:** select a shutter, scrub or type µs → servo moves (set-and-go); save into the
  four slots persists to NVS (survives a FS OTA); never appears on the everyday dashboard.

---

## Open decisions

| # | Decision | Notes |
| - | -------- | ----- |
| D1 | Final horn/arm ratio | Resolved by Phase 0 force test; start 10 mm horn + 20 mm arm. **Assumption to bench-validate (2026-07-08):** the rod end appears to need ~36 mm of rise *and* ~36 mm of horizontal excursion over 0→90° — an effective crank radius of ~36 mm (tip travels a ~51 mm chord), not 20 mm. If confirmed, a 1:1 parallelogram linkage needs a ~36 mm servo horn, and MG90D torque at that radius (~2.2 kg·cm → ~0.6 kg force) must be rechecked against the measured slat load. Use the v0.2.1 speed slider to run 0→90° slowly and measure actual rod displacement. |
| D2 | Servo connectors in hub | Grommet holes + strain relief (prototype) vs JST-XH (tidy/serviceable). [hardware-layout.md](hardware-layout.md) recommends grommets for the prototype, JST-XH panel breakout revisited at Phase 7. |
| D3 | Auto-OTA vs web-OTA only | Web OTA (ElegantOTA) certain; `latest.json` auto-update optional. |
| D4 | Spare LM2596 | Superseded by XL4015; keep as spare or drop from BOM. |
| D5 | Voltage monitoring | Whether to populate GPIO34 servo-rail ADC for brownout telemetry. |
| D6 | Config editing UX | Resolved (ADR 0005): browser-owned web form, servos declared manually (no auto-scan); HA-side config is a later phase. |
| D7 | HA integration form | Resolved (ADR 0006): MQTT Discovery only — no published `custom_components`/HACS integration. Broker-free HTTP-native integration deferred as an optional path, not planned work. |
| D8 | HA card home | Resolved (ADR 0007): custom Lovelace **frontend** card (control + calibration), not stacked built-in cards. **Open sub-choice:** ship from a top-level `ha-card/` in this repo vs a companion `esp32-shutter-hub-card` repo for a clean HACS frontend listing — pick per how HACS should index it. |
| D9 | Servo position feedback | v0.3.0 handles the open-loop first-move jump in software (NVS position memory + assembly "home", [ADR 0009](decisions/0009-servo-position-memory.md)). **Open:** whether to ever add true feedback — tap the servo pots into an **ADS1115** (0x48) on the shared I2C bus, or move to feedback/serial-bus servos — to also kill the jump after a slat is moved by hand while unpowered. Deferred; revisit at Phase 7 if the residual proves annoying. |
