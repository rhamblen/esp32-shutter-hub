# 0005 — MQTT command structure & config ownership for Home Assistant

- **Status:** Accepted
- **Date:** 2026-07-07

## Context

v0.2.0 adds MQTT + Home Assistant discovery to the hub. Before wiring per-shutter
control we had to settle: (a) who owns configuration — the hub's web UI or Home
Assistant; (b) the command vocabulary exposed to HA; (c) how "favourite" positions
work; (d) whether the hub can discover how many servos are attached.

The shutters are dumb PWM servos on a PCA9685. A servo signal line has **no
back-channel**, so the hub cannot detect servo presence, count, or health.

## Decision

**Config ownership — browser-owned (device is source of truth).**
The hub web UI owns hub and per-servo configuration; MQTT discovery only
*advertises* that config to Home Assistant. A richer HA-side config tool is a later
phase, not v0.2.x.

**Servo inventory — manual.** Servo count and PCA9685 channel mapping are declared
by the user in the web UI. No auto-scan (hardware cannot support it).

**Config storage.** Web assets live in LittleFS (`data/`). Hub identity, MQTT
credentials, and **shutter definitions + calibration + favourites live in NVS
(Preferences)** so a filesystem OTA re-flash never wipes them.

**Command model.** Each shutter is one Home Assistant `cover` (position 0–100 = slat
angle, closed→open), plus custom commands surfaced as HA `button` entities.

Base topic default `shutter-hub`; per-shutter id is a slug (e.g. `front_left`).

| Action | Canonical | Topic (device subscribes) | Payload | HA entity |
| ------ | --------- | ------------------------- | ------- | --------- |
| Open   | Open      | `<base>/cover/<id>/set`          | `OPEN`         | cover |
| Close  | Close     | `<base>/cover/<id>/set`          | `CLOSE`        | cover |
| Pause  | Stop      | `<base>/cover/<id>/set`          | `STOP`         | cover |
| Slider | Set position | `<base>/cover/<id>/position/set` | `0–100`     | cover |
| Up     | Jog open (step)  | `<base>/cover/<id>/cmd`   | `jog_open`     | button |
| Down   | Jog close (step) | `<base>/cover/<id>/cmd`   | `jog_close`    | button |
| Set    | Save favourite   | `<base>/cover/<id>/cmd`   | `save:daylight`\|`save:privacy`     | button |
| Move to memory | Recall favourite | `<base>/cover/<id>/cmd` | `recall:daylight`\|`recall:privacy` | button |

The left "Action" column records the colloquial request terms only for traceability;
the **canonical** name is what appears in the UI, firmware identifiers, and payloads.

State (retained, device publishes): `<base>/cover/<id>/position` → `0–100`;
`<base>/cover/<id>/state` → `opening|closing|open|closed|stopped`.
Availability: hub-wide `<base>/status` (LWT `online`/`offline`).
Discovery prefix: `homeassistant/{cover,button,sensor}/<hub>/<id>/config`.

**Favourites — two named presets: Daylight & Privacy.** "Set" saves the current
angle into a preset; "Recall" moves to it. Names chosen to match the Phase 6
solar-protection positions rather than anonymous numbered slots.

**Up/Down — incremental jog.** Each command nudges the slats one step; Open/Close go
fully to the calibrated limits.

## Consequences

- v0.2.0 ships broker connection + hub *diagnostic* discovery (rssi/uptime) only.
  The cover/button entities and the `cmd` handler land in **Phase 4**, once
  `ServoController` drives real per-channel positions.
- The web UI needs a **Shutters** config surface (count, names, channels,
  calibration, Daylight/Privacy presets) — added alongside Phase 4.
- HA users get native Open/Close/Stop/position plus buttons for jog and presets.
- Because favourites are named (not numbered), the solar automation in Phase 6 can
  target `privacy`/`daylight` directly.
- No auto-detection means a mis-configured channel is a user error surfaced only by
  the servo not moving — the Actions/diagnostics page should make channel testing easy.
