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
| `docs/decisions/0001–0010` | ADRs: hub, MG90D, XL4015+PCA9685, custom firmware, MQTT commands, MQTT-only HA, Lovelace card, build variants, position memory, concurrent drive |
| `firmware/data/` | LittleFS web UI (index.html, style.css, app.js) served by `WebUI` |
| `CHANGELOG.md` | Keep-a-Changelog; update every phase |

## Locked facts

- **Shutters:** 4 panels, ~450 mm wide, 11 slats, ~75 mm slat height, edge tilt rod, lightweight.
- **Servo:** MG90D (digital metal gear, ~2 kg·cm, ~13–14 g, 4.8–6 V).
- **Driver:** PCA9685 (I2C 0x40, CH0–3).
- **Sensor:** VEML7700 (I2C 0x10, solar heat protection).
- **Power:** USB-C PD → AITRIP trigger (12 V) → XL4015 @ **5.1 V** → ESP32 + PCA9685 + servo rail;
  1000–2200 µF bulk cap; common ground. XL4015 replaced LM2596.
- **Linkage:** M2×50 mm ball-link pushrod (hole-to-hole 68–78 mm); horns 8/10/12 mm; printed arm
  holes 15/20/25 mm; start 10 mm horn + 20 mm arm. **Under bench validation (project-plan D1):** rod
  end appears to need ~36 mm rise + ~36 mm out over 0→90° → effective crank radius ~36 mm, not 20 mm.
- **Pinout:** GPIO21 SDA, GPIO22 SCL, GPIO34 optional servo-rail ADC, GPIO0 boot.
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
- **Solar:** trip lux>60000/10min → Privacy; clear lux<30000/20min → Daylight.
- **Manual override:** suspend automation 2 h after a manual move, per shutter.

## Build phases

Done: **S** (v0.0.1–3 framework + WiFi + OTA), **1** (v0.1.0 single-servo bench test; v0.2.1 adds a
persisted **speed slider** 5–120 °/s default 25, `POST /api/servo/speed?dps=N`, slewed moves),
**S2** (v0.2.0 LittleFS web UI + WebSocket logs + MQTT/HA config), **2** (v0.2.2–v0.3.0 Shutters
page + calibration; PCA9685 backend + build variants + position memory). Phase 3 retired (folded
into S/S2), **4** (v0.4.0 MQTT/HA covers + buttons + discovery + concurrent drive; verified on
hardware), **4b** (v0.4.2 Lovelace operating card). **5 groundwork** shipped in v0.4.4: System ▸
HomeKit config tab (`/api/homekit`, NVS `hk*` keys, default setup code **748-88-377** = "SHUTTERS"
on a keypad, client-side `X-HM://` pairing QR). The HomeSpan bridge itself is the remaining Phase 5
work — contract in `firmware/include/HomeKit.h`: QR setup ID **`SHUT`** (`homeSpan.setQRID`), HAP
off port 80 (`homeSpan.setPort(1201)`), WiFi stays with WiFiManager. Remaining: **5** HomeKit →
**6** light/solar → **7** production → **8** HA calibration card (optional). **0** (mechanical
force test) still open.

## Gotchas

- Set XL4015 to 5.1 V before wiring anything.
- Stagger servo start-up; keep bulk cap; ESP32 on the rail, not through the board; vent the XL4015.
- Stepped `moveTo`, never instant `write`.
- Calibration is per-shutter — panels differ.
- I2C: PCA9685 0x40 + VEML7700 0x10 share one bus, no clash.
