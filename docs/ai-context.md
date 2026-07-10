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
| `docs/decisions/0001–0011` | ADRs: hub, MG90D, XL4015+PCA9685, custom firmware, MQTT commands, MQTT-only HA, Lovelace card, build variants, position memory, concurrent drive, dedicated sensor I2C bus |
| `firmware/data/` | LittleFS web UI (index.html, style.css, app.js) served by `WebUI` |
| `CHANGELOG.md` | Keep-a-Changelog; update every phase |

## Locked facts

- **Shutters:** 4 panels, ~450 mm wide, 11 slats, ~75 mm slat height, edge tilt rod, lightweight.
- **Servo:** MG90D (digital metal gear, ~2 kg·cm, ~13–14 g, 4.8–6 V).
- **Driver:** PCA9685 (I2C 0x40, CH0–3).
- **Sensor:** VEML7700 (I2C 0x10) on its **own bus `Wire1`** — pins configurable, default SDA 25 /
  SCL 26 ([ADR 0011](decisions/0011-dedicated-sensor-i2c-bus.md)). **Not** on the PCA9685 bus.
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
- **Solar (ADR 0011, v0.6.0):** `LightSensor` (VEML7700 on `Wire1`, lean vendored driver, fixed
  gain 1/8 + IT 25 ms → ~1.8432 lx/ct, full scale ≈120 k lx, **linear lux — no Vishay correction**,
  so reported lux is approximate) + `SolarLogic` (`idle→counting-trip→tripped→counting-clear`).
  Defaults: trip >60000 lx for 10 min → **Privacy**; clear <30000 lx for 20 min → **Do nothing**.
  Both targets are one of **open|closed|daylight|privacy|none** (`AppConfig::SolarTarget`, 0–4);
  `none` advances state but drives nothing. Solar config applies **without a reboot**.
  Web: **Solar** page + `/api/solar`, `/api/solar/simulate` (simulate-lux slider works with no
  sensor fitted). MQTT: `<base>/solar/{lux,state}` published retained; `<base>/solar/{enable,
  trip_lux,clear_lux}/set` subscribed → HA illuminance + state sensors, an automation switch and two
  writable `number` thresholds (clear ≥ trip is rejected). Card keys: `solar_switch`/`solar_lux`/
  `solar_state`, all optional.
- **Manual override:** a web recall or an MQTT set/position/jog/recall calls
  `SolarLogic::notifyManualMove(id)` → automation suspended **2 h on that shutter** (ephemeral, not
  persisted). SolarLogic's own moves go straight to `ServoController::moveSlotUs` and never self-suspend.

## Build phases

Done: **S** (v0.0.1–3 framework + WiFi + OTA), **1** (v0.1.0 single-servo bench test; v0.2.1 adds a
persisted **speed slider** 5–120 °/s default 25, `POST /api/servo/speed?dps=N`, slewed moves),
**S2** (v0.2.0 LittleFS web UI + WebSocket logs + MQTT/HA config), **2** (v0.2.2–v0.3.0 Shutters
page + calibration; PCA9685 backend + build variants + position memory). Phase 3 retired (folded
into S/S2), **4** (v0.4.0 MQTT/HA covers + buttons + discovery + concurrent drive; verified on
hardware), **4b** (v0.4.2 Lovelace operating card). **5 HomeKit** done (v0.5.0): HomeSpan bridge
(`firmware/src/HomeKit.cpp`), one Window Covering accessory per shutter driving the same
`ServoController` slots as MQTT; config tab shipped in v0.4.4 (`/api/homekit`, NVS `hk*` keys,
default setup code **748-88-377**, client-side `X-HM://` QR). **Pinned `HomeSpan @ ~1.9.1`** — last
line supporting arduino-esp32 **core 2.0.9** (2.x needs core ≥3.3.0; do NOT bump the core, it would
churn every other lib). Coexistence (all in HomeKit.cpp): HAP `setPortNum(1201)` (web keeps 80),
`setHostNameSuffix("")` pins mDNS host to the device name + re-adds the `_http` service, WiFi stays
with WiFiManager (HomeSpan sees `WL_CONNECTED`, never calls WiFi.begin), `setQRID("SHUT")` +
`setPairingCode(code,false)`. **Gotcha: the pairing verifier is baked at boot** — a code/enable/name
change needs a reboot (the tab has *Reboot to apply*; the active code is logged to the web Logs
page). Uncalibrated shutters still operate via the servo envelope (MVP). **6 light/solar** done
(v0.6.0): `LightSensor` + `SolarLogic` + Solar page + MQTT solar entities + card header toggle —
see the Solar locked fact above and [ADR 0011](decisions/0011-dedicated-sensor-i2c-bus.md).
**Flash now 91.2 %** of the app partition — treat any new library as suspect; that's why the
VEML7700 driver is hand-rolled rather than Adafruit's. Remaining: **7** production → **8** HA
calibration card (optional) → **8b** HA-side threshold calibration from lux history (optional).
**0** (mechanical force test) still open. **Not yet verified against the physical VEML7700** — the
sensor isn't wired; everything was exercised through the simulate-lux slider.

## Gotchas

- Set XL4015 to 5.1 V before wiring anything.
- Stagger servo start-up; keep bulk cap; ESP32 on the rail, not through the board; vent the XL4015.
- Stepped `moveTo`, never instant `write`.
- Calibration is per-shutter — panels differ.
- **I2C: two separate buses.** PCA9685 0x40 on `Wire` (21/22); VEML7700 0x10 on `Wire1` (25/26).
  They *could* share one bus (distinct addresses) — ADR 0011 deliberately chose not to, so a
  sensor-lead fault can't wedge the servo driver. Don't "simplify" them back together.
- `Wire.begin()` only runs inside `#if USE_PCA9685` — a `-direct` build has **no** shared bus.
