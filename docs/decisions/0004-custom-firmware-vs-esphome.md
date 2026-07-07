# 0004 — Custom firmware vs ESPHome

- **Status:** Accepted
- **Date:** 2026-07-07

## Context

The hub could run **ESPHome** (fast HA integration, YAML config) or **custom Arduino/ESP-IDF
firmware**. Requirements include native Apple HomeKit, smooth/slow calibrated servo motion, a
bespoke calibration web UI, favourite positions, and full OTA control.

## Decision

Write **custom appliance-style firmware** (Arduino Core), with **HomeSpan** for HomeKit and **MQTT
discovery** for Home Assistant. Do **not** use ESPHome.

## Rationale

- **Native HomeKit** is a stated goal; ESPHome has no native HomeKit (would need a HA bridge).
- Need **custom smooth, stepped servo movement** and staggered start-up — awkward in ESPHome.
- Need a **bespoke calibration UI** (jog, SET CLOSED/OPEN, named favourites) and per-shutter µs
  storage.
- Full control over **OTA** (ElegantOTA browser upload + optional `latest.json` auto-update) and
  diagnostics (watchdog, servo timeout, brownout).

## Reference implementation

[**HomeKey-ESP32**](https://github.com/rednblkx/HomeKey-ESP32) is the pattern the user pointed to
and it independently validates this direction — it delivers HomeKit + web-config + OTA on an ESP32:

| Concern | HomeKey-ESP32 | This project |
| ------- | ------------- | ------------ |
| Apple Home | **HomeSpan** (credited as its HomeKit framework) | HomeSpan ✅ |
| OTA | web-interface OTA (specific lib unstated) | ElegantOTA browser upload |
| Web config | Svelte 5 + Tailwind SPA | lightweight embedded HTML/CSS/JS |
| Framework | ESP-IDF (`idf.py`) | Arduino Core |

Takeaway: **HomeSpan is the proven route to native Apple Home on ESP32.** HomeKey-ESP32 uses
ESP-IDF only because its NFC HomeKey reverse-engineering (`HK-HomeKit-Lib`) needs lower-level
access we don't require. HomeSpan is fundamentally an Arduino library, so staying on **Arduino
Core** is the simpler path to the same HomeKit result. Adopt HomeKey-ESP32 as a structural
reference (HomeSpan setup, web-OTA UX) without inheriting its ESP-IDF/Svelte complexity.

## Consequences

- More firmware to build/maintain than an ESPHome YAML (see the phased structure in
  [project-plan.md](../project-plan.md)).
- Structured modules: `WiFiSetup`, `WebServer`, `Ota`, `ServoController`, `HomeKit`, `Mqtt`,
  `LightSensor`, `Config`, `Diagnostics`.
- HA integration is via MQTT discovery rather than the ESPHome API.
- If HomeKit is later dropped, ESPHome could be reconsidered to cut maintenance — revisit then.
