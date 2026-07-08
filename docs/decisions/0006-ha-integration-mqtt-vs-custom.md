# 0006 — Home Assistant integration: MQTT Discovery, not a published custom integration

- **Status:** Accepted
- **Date:** 2026-07-08

## Context

[0004](0004-custom-firmware-vs-esphome.md) chose custom firmware with **MQTT
discovery** as the Home Assistant path, and [0005](0005-mqtt-command-structure.md)
fully specified the topic contract (cover/button/sensor entities, state, LWT
availability, `homeassistant/.../config` discovery payloads). A separate question
remained: should we *also* build and publish a custom HA integration
(`custom_components/`, config flow, distributed via HACS) on top of — or instead
of — MQTT?

## Decision

**Ship MQTT Discovery only. Do not build a published custom integration.**

The retained discovery config topics defined in 0005 *are* the integration: they
give HA native `cover` entities (position, open/close/stop), `button` entities
(jog, `save:`/`recall:` presets), and hub diagnostic `sensor`s with no HA-side
install step.

## Rationale

- **Zero-install** — entities appear automatically when the hub connects to the
  broker; a custom integration needs installation + a config flow for no functional
  gain.
- **No Python component to maintain** — a custom integration is a separate codebase
  that breaks on HA core releases (config-flow API churn, quality-scale rules). MQTT
  discovery has no such tax.
- **Matches config ownership** — 0005 makes the hub's web UI the source of truth and
  HA a consumer. Discovery *advertises* config; it does not tempt splitting config
  ownership back into HA.
- **Non-standard commands already covered** — jog and named presets map to plain
  MQTT `button` entities, so we never hit the wall (rich services / options flow)
  that would justify a custom component.

## Consequences

- Requires the user to run an **MQTT broker** (e.g. Mosquitto / the HA add-on). This
  is the one accepted cost.
- **Deferred optional path:** for broker-averse users, an HTTP/WebSocket-native
  HACS integration talking to the hub's existing API could be added later. It is
  *not* planned work — it would require a second transport in firmware and ongoing
  Python maintenance, and is only revisited if broker-free installs become a real
  demand.
- No published `custom_components/` or HACS repo for this project at this time.
