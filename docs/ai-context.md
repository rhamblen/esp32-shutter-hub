# AI Context — cold-start orientation

Dense factual map for the next AI session. Not for end users. Read this first.

## Purpose

Automate 4 existing Front Room plantation shutters (tilt-only) with one **ESP32 Shutter Hub**
driving a **variable** number of **MG90D** servo actuators via a **PCA9685**, integrated with
**Apple HomeKit (HomeSpan)** and **Home Assistant (MQTT)**. Replaces a Zemismart JM36/JC601.

## How to work here

- The **repo is the source of truth for intent + rationale.** Update docs when decisions change.
- Architecture and part choices are **locked via ADRs** in `docs/decisions/` — don't silently
  re-open them; if reversing, write a new ADR.
- Shutter count is **configuration, not code** — never hard-code "4".
- Follow the user's phased-repo doc conventions (status table, ADRs, CHANGELOG per phase).

## File map

| File | Contents |
| ---- | -------- |
| `README.md` | Shop-window overview |
| `docs/project-brief.md` | Master engineering spec (mechanical + electronics + firmware) |
| `docs/project-plan.md` | Phased roadmap + status table + open decisions |
| `docs/architecture.md` | Principles, trade-off table, topology, gotchas |
| `docs/inventory.md` | Shutter facts + BOM + power budget |
| `docs/hardware-layout.md` | Copper-breadboard build plan: placement, cuts, standoffs, cables, connectors |
| `docs/installation.md` | End-user install guide: flash → WiFi → calibrate → HA → HomeKit → solar |
| `docs/user-guide.md` | Everyday operation, solar behaviour, recalibration, troubleshooting |
| `docs/pinout.md` | GPIO map: ESP32-D defaults, C3 proposal, pins `validGpio()` rejects |
| `docs/ha-lovelace-card.md` | Lovelace card spec: entity model, config schema, layout |
| `docs/decisions/0001–0012` | ADRs: hub, MG90D, XL4015+PCA9685, custom firmware, MQTT commands, MQTT-only HA, Lovelace card, build variants, position memory, concurrent drive, dedicated sensor I2C bus, selectable sensor bus |
| `firmware/data/` | LittleFS web UI (index.html, style.css, app.js, qrcode.min.js) served by `WebUI` |
| `ha-card/shutter-hub-card.js` | The shipped Lovelace card (plain custom element, no build step) |
| `CHANGELOG.md` | Keep-a-Changelog; update every phase |
| `RELEASE_NOTES.md` | Body of the **current** GitHub release (rewritten each release) |

## Locked facts

- **Shutters:** 4 panels, ~450 mm wide, 11 slats, ~75 mm slat height, edge tilt rod, lightweight.
  Count is configuration, but `Shutters::MAX` (`firmware/include/Shutters.h`) currently caps it at
  **4** — iterate the array, never hard-code 4, and raise `MAX` if more are needed (Phase 8 wants 6).
- **Servo:** MG90D (digital metal gear, ~2 kg·cm, ~13–14 g, 4.8–6 V).
- **Driver:** PCA9685 (I2C 0x40, CH0–3).
- **Sensor:** VEML7700 (I2C 0x10). Its bus is a **setting** ([ADR 0012](decisions/0012-selectable-sensor-i2c-bus.md)):
  **dedicated `Wire1`** (default, SDA 25 / SCL 26, fault-isolated per [ADR 0011](decisions/0011-dedicated-sensor-i2c-bus.md))
  or **shared** with the PCA9685 on `Wire` (21/22). Dedicated needs `SOC_I2C_NUM > 1` — true on the
  ESP32-D, **false on the C3**, where the preference is clamped to shared.
- **Power:** USB-C PD → AITRIP trigger (12 V) → XL4015 @ **5.1 V** → ESP32 + PCA9685 + servo rail;
  1000–2200 µF bulk cap; common ground. XL4015 replaced LM2596.
- **Linkage:** M2×50 mm ball-link pushrod (hole-to-hole 68–78 mm); horns 8/10/12 mm; printed arm
  holes 15/20/25 mm; start 10 mm horn + 20 mm arm. **Under bench validation (project-plan D1):** rod
  end appears to need ~36 mm rise + ~36 mm out over 0→90° → effective crank radius ~36 mm, not 20 mm.
- **Pinout:** GPIO21 SDA + GPIO22 SCL (`Wire`, PCA9685); GPIO25 SDA + GPIO26 SCL (`Wire1`,
  VEML7700, configurable); GPIO34 optional servo-rail ADC, GPIO0 boot. GPIO34–39 are input-only —
  they can never carry I2C.
- **Firmware:** custom Arduino (not ESPHome); HomeSpan bridge + MQTT (PubSubClient); ESPAsyncWebServer
  + LittleFS SPA + `/ws/logs` WebSocket; WiFiManager; custom OTA (firmware + LittleFS);
  `ServoController.moveTo()` smooth/staggered.
- **MQTT/HA (ADR 0005, implemented v0.4.0):** browser-owned config; servos declared manually (no PWM
  auto-scan); each shutter = one HA `cover` (position 0–100 = slat angle) on `<base>/cover/<id>/…` +
  six `button`s (jog_open/jog_close on `cmd`, `recall:`/`save:` `daylight`/`privacy`); state topics
  `position` + `state` retained; hub-wide LWT `<base>/status`. Jog = 25 µs/press, clamped to
  calibration. Uncalibrated ⇒ reports 0/closed, OPEN/CLOSE/position ignored (logged). Discovery
  re-publishes on shutter config change. Phase-8 calibration cmds (`goto_us`, `save:open/close`) deferred.
  Config in NVS, web assets in LittleFS.
- **Servo drive (ADR 0010, v0.4.0):** per-slot slew state — all channels move concurrently at the
  shared speed; bench API acts on the active test channel; slot API (`moveSlotUs` …) for MQTT.
- **Solar (ADR 0011 + 0012, v0.6.0/v0.6.1):** `LightSensor` (VEML7700 on the selected bus, lean vendored driver, fixed
  gain 1/8 + IT 25 ms → ~1.8432 lx/ct, full scale ≈120 k lx, **linear lux — no Vishay correction**,
  so reported lux is approximate) + `SolarLogic` (`idle→counting-trip→tripped→counting-clear`).
  Defaults: trip >60000 lx for 10 min → **Privacy**; clear <30000 lx for 20 min → **Do nothing**.
  Both targets are one of **open|closed|daylight|privacy|none** (`AppConfig::SolarTarget`, 0–4);
  `none` advances state but drives nothing. Solar config applies **without a reboot**.
  Web: **Solar** page + `/api/solar`, `/api/solar/simulate` (simulate-lux slider works with no
  sensor fitted). MQTT: `<base>/solar/{lux,brightness,state}` published retained; `<base>/solar/{enable,
  trip_lux,clear_lux}/set` subscribed → HA illuminance + brightness-% + state sensors, an automation
  switch and two writable `number` thresholds (clear ≥ trip is rejected). Card keys:
  `solar_switch`/`solar_lux`/`solar_state`, all optional.
- **`brightness` is display-only** (`LightSensor::brightnessPct()`): `20 × log10(clamp(lux,1,1e5))`,
  one lux decade per 20 points — 0 dark … 100 full sun. Perceptual, not linear (a linear % of the
  120 k full scale reads 0 % indoors). It **compresses the 30–60 k trip band to 90 %–96 %**, so the
  state machine trips on raw lux and never on this. Raw `lux` also stays as the Phase-8b history source.
- **Info page hardware table (v0.6.2):** `GET /api/info` carries `servo` (`usesPca,pin,channel,sda,
  scl,attached`), `sensor` (`enabled,present,bus,sda,scl,dedicatedSupported` — pins are the **active**
  ones, post-clamp), `homekit` (`enabled,running,paired,controllers`) and `shutters`
  (`name,channel,calibrated`). `renderHw()` in `data/app.js` draws one row per physical device; it is a
  **read-only mirror** — pins are set on Servo test, channels on Shutters, the sensor bus on Solar.
  On a PCA9685 build `ServoController::pin()` aliases the channel, so the UI branches on `usesPca`.
- **Manual override:** a web recall or an MQTT set/position/jog/recall calls
  `SolarLogic::notifyManualMove(id)` → automation suspended **2 h on that shutter** (ephemeral, not
  persisted). SolarLogic's own moves go straight to `ServoController::moveSlotUs` and never self-suspend.

## Build phases

Done: **S** (v0.0.1–3 framework + WiFi + OTA), **1** (v0.1.0 single-servo bench test; v0.2.1 adds a
persisted **speed slider** 5–120 °/s default 25, `POST /api/servo/speed?dps=N`, slewed moves),
**S2** (v0.2.0 LittleFS web UI + WebSocket logs + MQTT/HA config), **2** (v0.2.2–v0.3.0 Shutters
page + calibration; PCA9685 backend + build variants + position memory). Phase 3 retired (folded
into S/S2), **4** (v0.4.0 MQTT/HA covers + buttons + discovery + concurrent drive; verified on
hardware), **4b** (v0.4.2 Lovelace operating card). **5 HomeKit** ◐ **built, pairing unresolved**
(v0.5.0–v0.5.4, released as v0.5.4): HomeSpan bridge
(`firmware/src/HomeKit.cpp`), one Window Covering accessory per shutter driving the same
`ServoController` slots as MQTT; config tab (`/api/homekit`, NVS `hk*` keys,
default setup code **748-88-377**, client-side `X-HM://` QR). **No controller has ever paired** —
the bridge boots and advertises, the hub stays fully functional with it enabled, but *Add Accessory*
never completes. v0.5.1–v0.5.4 each fixed a real defect (reboot via `esp_timer`; HomeSpan on its own
FreeRTOS task; single mDNS owner) without producing a pairing. **Parked** — do not assume HomeKit
works. **Pinned `HomeSpan @ ~1.9.1`** — last
line supporting arduino-esp32 **core 2.0.9** (2.x needs core ≥3.3.0; do NOT bump the core, it would
churn every other lib). Coexistence (all in HomeKit.cpp): HAP `setPortNum(1201)` (web keeps 80),
`setHostNameSuffix("")` pins mDNS host to the device name + re-adds the `_http` service, WiFi stays
with WiFiManager (HomeSpan sees `WL_CONNECTED`, never calls WiFi.begin), `setQRID("SHUT")` +
`setPairingCode(code,false)`. **Gotcha: the pairing verifier is baked at boot** — a code/enable/name
change needs a reboot (the tab has *Reboot to apply*; the active code is logged to the web Logs
page). Uncalibrated shutters still operate via the servo envelope (MVP). **6 light/solar** ◐ built
(v0.6.0–v0.6.2): `LightSensor` + `SolarLogic` + Solar page + MQTT solar entities + card header toggle
(v0.6.0); selectable sensor bus (v0.6.1, [ADR 0012](decisions/0012-selectable-sensor-i2c-bus.md));
Info-page hardware table + brightness-% sensor (v0.6.2) — see the Solar locked fact above and
[ADR 0011](decisions/0011-dedicated-sensor-i2c-bus.md).
**Flash now 91.3 %** of the app partition (`esp32d-pca9685`; 91.9 % `esp32d-direct`, C3 envs link at
89.5 % / 90.1 %) — treat any new library as suspect; that's why the
VEML7700 driver is hand-rolled rather than Adafruit's. Remaining: **7** production → **8** HA
calibration card (optional) → **8b** HA-side threshold calibration from lux history (optional).
**0** (mechanical force test) still open. **Not yet verified against the physical VEML7700** — the
sensor isn't wired; everything was exercised through the simulate-lux slider.

## Gotchas

- Set XL4015 to 5.1 V before wiring anything.
- Stagger servo start-up; keep bulk cap; ESP32 on the rail, not through the board; vent the XL4015.
- Stepped `moveTo`, never instant `write`.
- Calibration is per-shutter — panels differ.
- **I2C: two buses by default.** PCA9685 0x40 on `Wire` (21/22); VEML7700 0x10 on `Wire1` (25/26).
  They *could* share one bus (distinct addresses); ADR 0011 chose not to so a sensor-lead fault
  can't wedge the servo driver. ADR 0012 made it a **setting** — don't hard-code either way.
- **Never `end()` the shared bus.** `LightSensor::reconfigure()` only ever calls `Wire1.end()`
  (guarded by `g_dedUp`). `Wire.end()` would deinit the PCA9685 and freeze every servo.
  `TwoWire::begin()` *is* idempotent on a started master bus, so shared mode needs no handshake.
- `Wire.begin()` only runs inside `#if USE_PCA9685` in ServoController — on a `-direct` build in
  shared mode, `LightSensor` is the first caller and brings `Wire` up itself.
- **`SOC_I2C_NUM` is 2 on the ESP32-D, 1 on the C3**, but the Arduino core declares `Wire1`
  unconditionally — so a C3 build *links* and fails silently at runtime. Capability must be gated
  on `SOC_I2C_NUM`, never on "it compiled".
- The **LittleFS image is shared across all variants**, so any per-chip/per-variant UI difference
  must come from the API at runtime (`usesPca`, `dedicatedSupported`), never from build-time markup.
