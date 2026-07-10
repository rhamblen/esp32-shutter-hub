# shutter-hub-card — Home Assistant Lovelace card

An operating tile for the ESP32 Shutter Hub: 1–6 named shutters side by side, the four
blind states (**Open · Close · Daylight · Privacy**) and a **manual position slider**,
applied to **all shutters at once** or to **one selected shutter**.

No build step — a single plain custom element (`shutter-hub-card.js`), registered as a
Lovelace resource. Full design rationale in
[../docs/ha-lovelace-card.md](../docs/ha-lovelace-card.md) and
[../docs/decisions/0007-ha-lovelace-card.md](../docs/decisions/0007-ha-lovelace-card.md).

## What it does

- **Group mode (default)** — every configured shutter shown as a tile (a slat glyph whose
  angle tracks position, its name, and state). The button bar and slider act on **all**
  shutters.
- **Individual mode** — tap a tile to scope the buttons and slider to just that shutter;
  tap it again, or **Select all**, to return to the group.
- **Four states** — Open / Close drive the cover to its calibrated limits; Daylight /
  Privacy press the saved-preset buttons.
- **Manual position** — the slider commands an arbitrary position (0–100 %) via
  `cover.set_cover_position`, for any angle that isn't one of the four presets.
- **Stop** — halts an in-flight move.

## Entities

Binds to the MQTT-discovered entities (one hub device):

| Entity | Role |
| ------ | ---- |
| `cover.<hub>_<id>` | open / close / stop / position |
| `button.<hub>_<id>_daylight` | recall the saved Daylight preset |
| `button.<hub>_<id>_privacy` | recall the saved Privacy preset |

The two button ids default to the cover id with `cover.` → `button.` and a
`_daylight` / `_privacy` suffix; override per shutter with `daylight:` / `privacy:` if
your naming differs.

### Solar heat protection (optional)

If the hub has a VEML7700 fitted, three more entities can be bound. Supply any of them and the
group header grows a **live light caption** and an **automation toggle**; omit them all and the
card renders exactly as before, so no-sensor installs are unaffected.

| Config key | Entity | Role |
| ---------- | ------ | ---- |
| `solar_switch` | `switch.<hub>_solar_automation` | Turns the trip/clear automation on or off |
| `solar_lux` | `sensor.<hub>_light_level` | Raw illuminance in lux — what the card's caption shows |
| `solar_state` | `sensor.<hub>_solar_state` | `idle` · `counting-trip` · `tripped` · `counting-clear` |

**Why the caption reads lux and not a percentage.** The hub also publishes
`sensor.<hub>_brightness` (0–100 %), which is friendlier to read but is **display-only**, and this
card does not use it. It is a logarithmic restatement of the same reading —
`20 × log10(lux)`, one lux decade per 20 points:

| Brightness | Lux | Roughly |
| ---------- | --- | ------- |
| 0 % | 1 | dark room |
| 20 % | 10 | dim |
| 40 % | 100 | lit room |
| 54 % | ~500 | bright room |
| 60 % | 1 000 | overcast, by a window |
| 80 % | 10 000 | overcast daylight |
| 100 % | 100 000 | full sun |

A linear percentage of the sensor's 120 000 lx full scale was rejected: it reads 0 % at every
indoor level and only moves in direct sun. The log curve fixes that, but it compresses the
30 000–60 000 lx band where the trip and clear thresholds live — the defaults land at 96 % and
90 %, six points apart — so it can't be used to set or reason about thresholds, and it isn't
cleanly invertible. **The state machine trips on raw lux**; raw lux is what the caption shows and
what Home Assistant's long-term statistics record. Treat brightness as a human-readable gauge,
never as an automation input.

```yaml
solar_switch: switch.shutter_hub_solar_automation
solar_lux: sensor.shutter_hub_light_level
solar_state: sensor.shutter_hub_solar_state
```

## Install

Two paths — both end with the same dashboard card.

**Inline resource (what this repo deploys):** register `shutter-hub-card.js` as a
dashboard resource of type `module` (the HA config API accepts inline module content), then
add the card below. No file hosting needed.

**Filesystem:** copy `shutter-hub-card.js` to `/config/www/shutter-hub-card.js`, add a
Lovelace resource `URL /local/shutter-hub-card.js`, type **JavaScript Module**, hard-refresh.

## Card config

```yaml
type: custom:shutter-hub-card
title: Shutters
shutters:                       # 1–6 entries, order = display order
  - entity: cover.shutter_hub_shutter_1
    name: Left                  # optional; defaults to the cover's friendly name
  - entity: cover.shutter_hub_shutter_2
    name: Right
```

## Not included (Phase 8, optional)

**Calibration / config card** — setting raw-µs positions, the full-open/close travel
endpoints, and *saving* into the four slots — is deliberately a **separate card**, planned
as **optional Phase 8** in [the project plan](../docs/project-plan.md). It needs firmware
commands that don't exist yet (an absolute *go-to-µs*, raw-µs readback, and `save:open` /
`save:close`), documented in [../docs/ha-lovelace-card.md](../docs/ha-lovelace-card.md) §4.
Until then, calibrate in the hub's web UI; this card operates the shutters HA already
knows how to drive.
