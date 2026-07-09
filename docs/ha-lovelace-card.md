# Home Assistant Lovelace card — build spec

Build-ready spec for the Shutter Hub dashboard cards. Companion to
[decisions/0007-ha-lovelace-card.md](decisions/0007-ha-lovelace-card.md) (why a custom
card) and [decisions/0005-mqtt-command-structure.md](decisions/0005-mqtt-command-structure.md)
(the entities/topics it binds to). Everything here is implementable as written; items
that need firmware are called out under **Firmware dependencies**.

Two cards are specified here. **Status:** the operating card
([`ha-card/shutter-hub-card.js`](../ha-card/shutter-hub-card.js)) **shipped in v0.4.2**;
the calibration card is **deferred** to a future phase because it needs firmware that
does not exist yet (see §4 and the plan).

| Card | Type | Status | Depends on |
| ---- | ---- | ------ | ---------- |
| `shutter-hub-card` | Everyday control | **Shipped v0.4.2** | Phase 4 covers/buttons only |
| `shutter-hub-calibration-card` | Setup / calibration | **Deferred** | Phase 4 **+** calibration transport (§4) |

---

## 1. Entity model (what the cards bind to)

Per shutter, from ADR-0005 MQTT discovery. `<id>` is the hub slug (e.g. `front_left`);
HA object-ids are prefixed by the hub name at discovery time.

Entity ids below are the **actual discovery names** (verified against a live hub).

| Entity | Example | Used by | Purpose |
| ------ | ------- | ------- | ------- |
| `cover.<hub>_<id>` | `cover.shutter_hub_shutter_1` | both | position 0–100, open/close/stop |
| `button.<hub>_<id>_daylight` | `button.shutter_hub_shutter_1_daylight` | control card | recall Daylight preset |
| `button.<hub>_<id>_privacy` | `button.shutter_hub_shutter_1_privacy` | control card | recall Privacy preset |
| `button.<hub>_<id>_jog_open` | — | (optional) | nudge open one step |
| `button.<hub>_<id>_jog_close` | — | (optional) | nudge close one step |
| `button.<hub>_<id>_save_daylight` | — | calibration | store current angle → Daylight |
| `button.<hub>_<id>_save_privacy` | — | calibration | store current angle → Privacy |
| `sensor`/`number` (calibration) | see §4 | calibration | raw µs read + set-and-go |

The **default card never needs raw µs** — it works entirely in cover position % plus
the four recall/open/close actions.

---

## 2. Card config schema + editor

```yaml
type: custom:shutter-hub-card
title: Living room shutters        # optional; header text
shutters:                          # 1–6 entries, order = display order
  - name: Left                     # display name (defaults to cover friendly_name)
    entity: cover.shutter_hub_front_left
  - name: Centre
    entity: cover.shutter_hub_front_centre
  - name: Right
    entity: cover.shutter_hub_front_right
```

**Visual editor** (`getConfigElement` + `getStubConfig`):

- a repeatable row list — **add / delete / drag-reorder**; enforce **1 ≤ rows ≤ 6**;
- each row: a text field (name) + an entity picker filtered to `cover.*`;
- `getStubConfig` pre-fills any `cover.*` entities whose object-id matches the hub
  prefix, so dropping the card on a board is near-zero-config.

The calibration card takes the **same `shutters` list** (so the picker/reorder logic is
shared) plus per-shutter references to the calibration entities from §4.

---

## 3. Default card — `shutter-hub-card`

Two modes in one card; the button bar is identical in both, only the **target** changes,
so nothing jumps around when you select/deselect.

### 3a. Group mode (default, nothing selected)

```
┌──────────────────────────────────────────────┐
│ ▤ Living room shutters              [4 online]│
│                                                │
│  ┌────┐   ┌────┐   ┌────┐   ┌────┐             │
│  │▤▤▤▤│   │≡≡≡≡│   │▦▦▦▦│   │████│   ← slat     │
│  │Left│   │Ctr │   │Rght│   │Bay │     glyph    │
│  │clsd│   │72% │   │45% │   │clsd│     +name+%  │
│  └────┘   └────┘   └────┘   └────┘             │
│                All shutters                    │
│   [⤒ Open] [⤓ Close] [☀ Daylight] [◎ Privacy] │
└──────────────────────────────────────────────┘
```

- **Tiles** — one per shutter, side by side; wrap to a second row past ~4 across so 5–6
  stay legible at HA's ~490 px column width. Each tile: a **slat glyph whose angle
  tracks `current_position`** (0 % = flat/closed, 100 % = edge-on/open), the display
  name, and the live state (`closed` / `NN% open`). Tap a tile → individual mode.
- **Group bar** — acts on **every** shutter in the list:
  - Open → `cover.open_cover` on each (`OPEN`)
  - Close → `cover.close_cover` on each (`CLOSE`)
  - Daylight → press each `recall_daylight` button
  - Privacy → press each `recall_privacy` button
- **Header chip** — `N online` derived from cover availability; shows a warning tint if
  any shutter is `unavailable`.

### 3b. Individual mode (a tile is selected)

```
┌──────────────────────────────────────────────┐
│ ▤ Living room shutters            [✕ Back all]│
│  ┌────┐   ┌════┐   ┌────┐   ┌────┐             │
│  │dim │   ║ CTR║   │dim │   │dim │   selected  │
│  └────┘   ╚════╝   └────┘   └────┘   ring+tint │
│  Centre  ├──────────●───────┤  72%   ← slider   │
│                 Centre only                    │
│   [⤒ Open] [⤓ Close] [☀ Daylight] [◎ Privacy] │
└──────────────────────────────────────────────┘
```

- selected tile gets an accent ring + tint; the others dim (still tappable to switch);
- a **position slider** appears (`cover.set_cover_position`, 0–100), debounced on
  release;
- the four buttons now scope to the selected shutter only;
- **✕ Back to all** returns to group mode. Tapping the selected tile again also deselects.

State is card-local (no HA input helper) — selection resets on reload, which is fine.

---

## 4. Calibration card — `shutter-hub-calibration-card`

Setup-only tile, meant for a hidden/admin dashboard. Mirrors the web Positions panel
([hardware-layout.md] web UI) minus the nudge/shuffle transport — the owner explicitly
does **not** want shuffle here.

```
┌──────────────────────────────────────────────┐
│ ⚙ Shutter calibration                  [setup]│
│  Shutter: [ Centre ▾ ]                         │
│                                                │
│            1480 µs      51% open               │
│   closed ├───────────●─────────────┤ open      │
│          950 µs                 2050 µs        │
│                                                │
│   Exact µs [ 1480 ]  [ Go ]                    │
│                                                │
│   Save current position as                     │
│   [⤒ Full open ] [⤓ Full close ]              │
│   [☀ Daylight  ] [◎ Privacy    ]              │
└──────────────────────────────────────────────┘
```

- **Shutter selector** — dropdown over the same `shutters` list.
- **µs scrub slider** — range 500–2500 µs, with a live µs readout and a derived
  `% open` (interpolated between the shutter's calibrated `closed`/`open` edges once
  known). Ends labelled with the current calibrated closed/open µs.
- **Exact µs field + Go** — type a precise value, press Go to jump there.
- **Save targets** — four buttons that store the *current* position into a slot:
  Full open, Full close, Daylight, Privacy.

### Set-and-go (the key interaction)

**On any position change, the servo physically moves to that position** so the installer
sees the real slat angle before saving:

- **Slider** — fires **on change/release** (not per drag frame): send one *go-to-µs*
  command with the released value. Debounce ~150 ms; coalesce to the latest value.
- **Exact µs + Go** — sends the *go-to-µs* command immediately with the typed value.
- The card reflects the device's reported µs back into the slider/readout so display and
  hardware stay in sync (handles the case where firmware clamps to a safe range).

Because moving a servo is a physical action, **set-and-go lives only in the calibration
card** (an admin surface); the everyday card never issues raw-µs motion.

### Firmware dependencies (calibration card only)

ADR-0005 has no absolute-µs command and does not publish raw µs. Add to the Phase 4
`cmd`/state handlers:

| Need | Proposed topic | Payload | Direction |
| ---- | -------------- | ------- | --------- |
| Go-to absolute µs | `<base>/cover/<id>/cmd` | `goto_us:<500–2500>` | HA → hub |
| Current µs (readback) | `<base>/cover/<id>/calibration` | `<µs>` (retained) | hub → HA |
| Set travel edge | `<base>/cover/<id>/cmd` | `save:open` \| `save:close` | HA → hub |

`save:daylight` / `save:privacy` already exist in ADR-0005. Surface the two calibration
state values as HA `sensor`/`number` entities via discovery so the card can bind to them
without a bespoke transport. **None of this blocks the default card.**

---

## 5. Repo layout + build

Frontend bundle, separate from firmware. Proposed home — a top-level `ha-card/` in this
repo (single source of truth) or a companion repo `esp32-shutter-hub-card` for a clean
HACS *frontend* listing; pick per how you want HACS to index it (ADR-0007 leaves this
open).

```
ha-card/
  src/
    shutter-hub-card.ts            # everyday control card + editor
    shutter-hub-calibration-card.ts
    slat-glyph.ts                  # shared position→slats renderer
    editor/                        # shared shutters-list editor element
  dist/shutter-hub-card.js         # built bundle registered as a Lovelace resource
  package.json                     # lit + rollup/vite build
  hacs.json                        # HACS frontend descriptor (if companion repo)
  README.md / INSTALLATION.md      # per HA-card doc conventions
```

- **Framework:** Lit (the HA-card norm); TypeScript; Rollup or Vite → one `dist/*.js`.
- **Registration:** Lovelace resource (module) → `custom:shutter-hub-card` /
  `custom:shutter-hub-calibration-card`.
- **Theming:** use HA CSS custom properties (`--primary-color`, `--card-background-color`,
  etc.) so it follows the user's theme; no hard-coded colours.
- **Docs:** follow the HA dashboard-card repo conventions (README shop-window,
  INSTALLATION workshop-manual, CHANGELOG is the only place versions live).

---

## 6. Exit criteria

**Default card (buildable now against planned Phase 4 entities):**

- ☐ add the card, pick 1–6 covers in the visual editor, reorder them;
- ☐ tiles render side by side with slat angle tracking position;
- ☐ group bar Open/Close/Daylight/Privacy drives all shutters;
- ☐ selecting a tile scopes the slider + buttons to that shutter; Back to all restores
  group mode;
- ☐ follows the active HA theme; degrades gracefully if a shutter is `unavailable`.

**Calibration card (gated on the §4 firmware additions):**

- ☐ select a shutter, scrub µs, or type an exact value → **servo moves (set-and-go)**;
- ☐ readout shows µs + derived % and reflects device-reported µs;
- ☐ Save into Full open / Full close / Daylight / Privacy persists to NVS (survives a FS
  OTA, per ADR-0005);
- ☐ never appears on the everyday dashboard.
