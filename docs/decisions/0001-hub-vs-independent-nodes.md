# 0001 — Shutter Hub vs. Independent ESP32 Nodes

- **Status:** Accepted
- **Date:** 2026-07-07

## Context

Two topologies were considered for controlling the four Front Room shutters:

1. **Independent nodes** — each shutter gets its own ESP32 + servo, identical firmware,
   config-only differences.
2. **Shutter Hub** — one ESP32 drives all four servos over a short cable loom; actuator modules
   are mechanical-only.

The four shutters are in a **single room, physically close together**.

## Decision

Adopt the **Shutter Hub** architecture.

## Rationale

- Servo runs are only 1–2 m within one room, so servo-PWM signal integrity and 5 V voltage drop
  are non-issues — the main argument for distributed nodes (long cabling) does not apply here.
- One ESP32 exposes 16 LEDC PWM channels; driving 4+ servos is trivial.
- **HomeSpan can run as a bridge exposing one `Window Covering` accessory per shutter**, so Apple
  Home still shows four independent covers (and HA gets four `cover.*` entities). The hub costs
  nothing in user experience.
- 4× fewer ESP32s, power supplies, OTA targets, `.local` hostnames, and web UIs to maintain.
- Genuine synchronised movement from a single controller.

## Consequences

- Servo power rail must be sized for simultaneous movement: **~3–4 A peak** for four MG90-class
  servos. The LM2596 (~3 A, heat-derated) is marginal → use a **5 A buck** for the servo rail
  and/or stagger servo start-up. Keep the ESP32 on a **separate small buck** to avoid brownout.
- A cable loom runs from the hub to each actuator (5 V / GND / PWM per servo).
- Firmware must be generic over shutter count (iterate a `shutters[]` config array).

## Deferred / future

Distributed WiFi actuator nodes remain viable for **other rooms** later. The per-shutter MQTT and
HomeSpan-accessory design already accommodates this without re-architecting the hub.
