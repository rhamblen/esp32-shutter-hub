# Project Brief — ESP32 Smart Shutter Hub

**Master engineering specification.** Reusable when restarting the project, briefing another
AI, starting CAD, designing the PCB, or beginning firmware. Written as a design specification,
not a summary.

> **Architecture (locked):** One central **Shutter Hub** (single ESP32) drives a *variable* number
> of **MG90D servo actuator modules** over a short cable loom via a PCA9685 driver. Currently 4
> shutters; expandable or reducible by configuration only. Supersedes the earlier
> "four independent ESP32 shutters" concept.
>
> Decisions of record: [0001 hub vs nodes](decisions/0001-hub-vs-independent-nodes.md) ·
> [0002 servo = MG90D](decisions/0002-servo-mg90d.md) ·
> [0003 power chain + PCA9685](decisions/0003-power-chain-xl4015-pca9685.md) ·
> [0004 custom firmware vs ESPHome](decisions/0004-custom-firmware-vs-esphome.md)

---

## Project Overview

Automate four existing internal plantation shutters in the Front Room, replacing the bulky
commercial **Zemismart JM36/JC601** all-in-one tilt driver with a compact, mains-free (wired 5 V),
smart-home-native system. A single wall-mounted hub tilts the shutter slats via servo-driven
mechanical linkages and provides:

- Apple HomeKit control (each shutter its own accessory)
- Home Assistant integration (one `cover.*` entity per shutter)
- Local web configuration + calibration interface
- OTA firmware updates
- Per-shutter calibration and named favourite positions
- Automatic solar/heat protection from a light sensor
- True multi-shutter synchronisation

Smaller, cheaper, no battery maintenance, and fully local.

---

## 1. Shutter Mechanical Design (the foundation)

The system automates **existing** shutters — it does not lift them, only rotates the slats.

### Installation

| Panel | Location                | Servo output (PCA9685) |
| ----- | ----------------------- | ---------------------- |
| 1     | Front Room Left         | CH0                    |
| 2     | Front Room Left Centre  | CH1                    |
| 3     | Front Room Right Centre | CH2                    |
| 4     | Front Room Right        | CH3                    |

### Panel construction (per panel)

- Width: **~450 mm**
- Slats: **11 per panel**
- Slat height: **~75 mm (3")**
- Material: lightweight wood or PVC blades, plastic tilt linkage
- **Each slat pivots about its centre** (pinned into the frame stiles). **One vertical tilt rod at
  the edge of the frame** links to a point just ahead of each pivot, so lifting/lowering the rod
  rotates all 11 slats together about their centres — the rod moves at one end; the slats turn about
  their middles.

Because the actuator only overcomes slat-rotation friction (not panel weight), the required force
is low — expected **< 1 kg** at the tilt rod. This makes a micro servo entirely adequate.

### Actuation concept

```text
MG90D servo → servo horn → M2 ball-link pushrod → ball joint → shutter tilt rod
```

The servo is mounted **beside/behind the frame edge**, parallel to it, driving the tilt rod through
a pushrod — not coupled directly to the rod (avoids the Zemismart mistake of routing all force
through the gears and demanding perfect alignment).

### Linkage geometry (mechanical advantage)

| Element             | Value                                   |
| ------------------- | --------------------------------------- |
| Servo horn radius   | 10 mm (horns drilled at 8 / 10 / 12 mm) |
| Shutter arm radius  | 20 mm (3D-printed arm holes: 15/20/25mm)|
| Ratio               | ~2:1 → half force, double travel        |
| Pushrod             | **M2 × 50 mm** ball-link (see §Parts)   |

Rough travel: servo ~90° with a 10 mm horn ≈ 16 mm pushrod travel → ≈ 8 mm at a 20 mm shutter arm —
enough to swing the tilt rod through its full range. The multi-hole arm lets you retune torque vs.
travel without redesigning parts:

| Servo horn | Shutter arm | Result           |
| ---------- | ----------- | ---------------- |
| 10 mm      | 25 mm       | maximum force    |
| 10 mm      | 20 mm       | balanced (start) |
| 10 mm      | 15 mm       | maximum travel   |

### Linkage parts (chosen)

- **M2 × 50 mm ball-link pushrod** — link-rod 50 mm; total adjustable 75–85 mm; **hole-to-hole
  68–78 mm** (this is the working ball-centre-to-ball-centre length). 304 stainless rod + nylon
  ball ends. A couple of **M2 × 60 mm** kept as backup for mounting flexibility.
- Threaded ends give length adjustment and low backlash; rod can be cut shorter if needed.

---

## 2. Servo (locked): MG90D

**MG90D digital metal-gear micro servo** — chosen over MG90S/analogue for digital control accuracy
and repeatability, and hugely over-margined vs. a DS3235-class servo (unnecessary here).

| Spec        | Approx value        |
| ----------- | ------------------- |
| Type        | Digital micro servo |
| Gears       | Metal               |
| Voltage     | 4.8–6 V             |
| Size        | ~23 × 12 × 29 mm    |
| Weight      | ~13–14 g            |
| Torque      | ~2 kg·cm class      |
| Control     | PWM (via PCA9685)   |

Torque check: ~1 kg force at a ~15–20 mm arm ≈ 15–20 kg·mm required vs. ~200 kg·mm available →
large safety margin. Final horn/arm ratio confirmed after the bench force test (see plan Phase 0).

---

## 3. System Architecture

```text
                    iPhone / Siri            Home Assistant
                         |                        |
                    Apple Home                 MQTT / API
                         | HomeKit                |
                         +-----------+------------+
                                     |
                            +------------------+
                            |   SHUTTER HUB     |  ESP32-D
                            |  Web UI · OTA     |  HomeSpan bridge (N accessories)
                            |  VEML7700 (I2C)   |  MQTT discovery
                            +--------+---------+
                                     | I2C (GPIO21/22)
                                +----------+
                                | PCA9685  |  16-ch servo driver
                                +----+-----+
             CH0        CH1        CH2        CH3   ... (variable)
              |          |          |          |
           MG90D      MG90D      MG90D      MG90D
           Left    Left Centre Right Ctr    Right
              |          |          |          |
          tilt rod   tilt rod   tilt rod   tilt rod
```

The hub is the single point of integration, configuration, OTA, and light sensing. Actuator
modules are mechanical-only (MG90D + horn + pushrod + printed bracket). Servo count is driven by a
config array, so any number of shutters (1 or more) works with the same firmware.

---

## 4. Power System (locked, ordered)

```text
USB-C PD charger (30W+)
      │
AITRIP PD trigger board  →  12 V
      │
XL4015 buck converter    →  set to 5.1 V   (≈5 A rated; replaces LM2596)
      │
      ├─ ESP32-D (VIN)          ~0.25 A
      ├─ PCA9685 (V+)           ~0.05 A
      └─ Servo rail ──┬── MG90D ×4   2–3 A peak (all moving)
                      │
                  1000–2200 µF electrolytic (≥10 V) near PCA9685
      Common ground throughout (single point)
```

- **XL4015 chosen over LM2596** for headroom: the LM2596 is realistically ~1.5–2 A (3 A marginal,
  heat-derated) — too close to the ~2–3 A servo peak. XL4015 gives ~1 A+ margin. Set output to
  **5.1 V** *before* connecting anything. (A spare LM2596 exists but is superseded.)
- Servos powered from the buck rail directly, **not** through the ESP32 board.
- Bulk capacitor on the servo rail is mandatory insurance against MG90D inrush dips.
- Enclosure must **vent above the XL4015** — it warms when all servos move together.

### Hub pinout (ESP32-D)

Defaults; all of them are runtime-configurable and persisted in NVS. Full table — including the
ESP32-C3 proposal and the pins the firmware refuses — in [pinout.md](pinout.md).

| Pin           | Function                                   |
| ------------- | ------------------------------------------ |
| GPIO21 (SDA)  | I2C `Wire` — **servo bus**, PCA9685 (0x40) |
| GPIO22 (SCL)  | I2C `Wire` clock                           |
| GPIO25 (SDA)  | I2C `Wire1` — **sensor bus**, VEML7700 (0x10) |
| GPIO26 (SCL)  | I2C `Wire1` clock                          |
| GPIO13        | Servo signal — `-direct` builds only        |
| GPIO34        | (optional) servo-rail voltage monitor (ADC, brownout detect) |
| GPIO0         | Boot / recovery                            |
| PCA9685 CH0–3 | Servo PWM → shutters 1–4                    |

The I2C addresses do not conflict, so the PCA9685 and VEML7700 *could* share one bus — but from
v0.6.0 they deliberately do not. The sensor runs on its own `Wire1`, so a fault on its long lead to
the window can never wedge the servo bus and strand the shutters. See
[ADR 0011](decisions/0011-dedicated-sensor-i2c-bus.md).

---

## 5. Light Sensor & Solar Protection

**VEML7700** ambient light sensor (I2C, 0–120,000 lux) at the hub. Purpose is **strong direct
sunlight detection for summer heat protection**, not general room lighting.

```
Trip:  lux > 60000 for 10 min  → move affected shutters to Privacy
Clear: lux < 30000 for 20 min  → return to Daylight
```

Thresholds/timers configurable. One sensor at the hub serves the whole room.

---

## 6. Calibration & Favourite Positions

Every panel differs slightly, so the hub stores **per-shutter** limits and named positions as
servo pulse widths (µs):

| Position   | Meaning              | Example |
| ---------- | -------------------- | ------- |
| `closed`   | Slats closed, no gap | 1620 µs |
| `privacy`  | Light, limited view  | 1750 µs |
| `daylight` | Normal daytime       | 1850 µs |
| `open`     | Max light/airflow    | 2190 µs |

Position % maps linearly between `closed` and `open`:
`pwm = closed + (open − closed) × (pos% / 100)` (e.g. 50 % of 1620–2190 → 1905 µs).

### Calibration web UI (per shutter)

```
Front Room Left        Current: 1850 µs
Jog: [-100][-10][-1][+1][+10][+100]
[SET CLOSED] [SET OPEN]
Favourites: [SET DAYLIGHT] [SET PRIVACY]
[SAVE]
```

---

## 7. Configuration Model

One JSON document on the hub describes the whole system; shutter count is data, not code:

```json
{
  "hostname": "shutter-hub",
  "mqtt": { "enabled": true, "host": "...", "topic_prefix": "shutterhub" },
  "solar": { "trip_lux": 60000, "trip_minutes": 10, "clear_lux": 30000, "clear_minutes": 20 },
  "shutters": [
    { "id": 1, "name": "Front Room Left",         "channel": 0, "invert": false,
      "cal": { "closed": 1620, "privacy": 1750, "daylight": 1850, "open": 2190 } },
    { "id": 2, "name": "Front Room Left Centre",  "channel": 1, "invert": false, "cal": {} },
    { "id": 3, "name": "Front Room Right Centre", "channel": 2, "invert": false, "cal": {} },
    { "id": 4, "name": "Front Room Right",        "channel": 3, "invert": false, "cal": {} }
  ]
}
```

Stored in LittleFS (JSON, web files) + Preferences (small calibration values). A single-shutter
install is just a one-element array.

---

## 8. Firmware Architecture (appliance-style, custom)

Not a flat Arduino sketch — structured like small appliance firmware. **Custom firmware
(HomeSpan + MQTT), not ESPHome** — chosen so we keep native HomeKit, custom smooth servo motion,
the calibration web UI, and full OTA control (see [ADR 0004](decisions/0004-custom-firmware-vs-esphome.md)).
Structural reference: [HomeKey-ESP32](https://github.com/rednblkx/HomeKey-ESP32) — a proven
HomeSpan + web-config + web-OTA design (we stay on Arduino Core rather than its ESP-IDF/Svelte stack).

- **Core:** Arduino Core for ESP32.
- **WiFi:** WiFiManager — first-boot AP + captive portal (`Shutter-Hub-Setup`), stores credentials.
- **Web:** ESPAsyncWebServer + AsyncTCP; HTML/CSS/JS in LittleFS. Single multi-shutter page +
  per-shutter calibration + OTA page.
- **HomeKit:** HomeSpan configured as a **bridge**, one `Window Covering` accessory per shutter.
- **HA:** MQTT discovery, one cover + sensors per shutter.
- **OTA:** ElegantOTA browser upload (`/update`) + optional auto-update checking a `latest.json`.
- **Config:** LittleFS JSON + Preferences.
- **Servo:** `ServoController` class exposing `moveTo(percent)` with soft-start / slow, stepped
  movement (not instant `write`), staggered start-up across channels to limit inrush.
- **Discovery:** mDNS → `shutter-hub.local`.

```
ShutterHub/
  src/  main.cpp  WiFiSetup.cpp  WebServer.cpp  Ota.cpp
        ServoController.cpp  HomeKit.cpp  Mqtt.cpp  LightSensor.cpp
        Config.cpp  Diagnostics.cpp
  data/ index.html  style.css  app.js
```

Firmware iterates the `shutters[]` array — generic over shutter count.

---

## 9. Web Interface

Single page at `http://shutter-hub.local`:

- **Status** — WiFi, per-shutter position, firmware version, live lux.
- **Control** — per shutter OPEN/CLOSE/STOP + 0–100 % slider; group **Open All / Close All /
  Daylight**.
- **Calibration** — per-shutter servo setup (§6).
- **OTA** — firmware upload.

```
Front Room Shutters
  Left          [====50%====] [Open][Close][Stop]
  Left Centre   [====50%====] [Open][Close][Stop]
  Right Centre  [====50%====] [Open][Close][Stop]
  Right         [====50%====] [Open][Close][Stop]
[Open All] [Close All] [Daylight]
```

---

## 10. Home Assistant Integration

MQTT discovery publishes per shutter:

```
cover.front_room_left / _left_centre / _right_centre / _right
sensor.shutter_hub_lux
sensor.front_room_left_position (etc.)
```

Commands: Open / Close / Stop / Set position / go-to favourite.

### Example automations
- **Morning** — sunrise + 20 min → Daylight.
- **Strong sun** — lux > 60000 AND temperature high → Privacy.
- **Evening** — sunset → Close.

---

## 11. Manual Override

Automation must not fight the user. On any manual move (web/HomeKit/HA), **suspend automation for
that shutter for 2 hours**, then resume. Tracked per shutter.

---

## 12. Safety Features

- **Watchdog** — recover from hangs.
- **Servo movement timeout** — max ~15 s of drive per move; stop + flag error if exceeded.
- **Movement limits** — hard min/max PWM per shutter; never drive past calibrated `closed`/`open`.
- **Brownout / staggered start-up** — ESP32 on isolated rail; stagger servo motion to limit inrush.

---

## 13. Enclosures

### Hub box (wall-mounted)
Compartments: USB-C entry → AITRIP trigger → XL4015 (**vent slots above**) → ESP32-D + PCA9685 →
servo cable exits ×N. **VEML7700 light window** on the face. Servo outputs 3-wire (+5 V / GND /
PWM); connectors either grommet holes + strain relief (prototype) or JST-XH (tidier) — see open
decisions.

```
+------------------------------------+
| USB-C                              |
| [AITRIP trigger]                   |
| [XL4015]   <-- ventilation slots   |
| [ESP32-D]  [PCA9685]               |
| Servo outputs:  o   o   o   o      |
|                S1  S2  S3  S4      |
| Light-sensor window (VEML7700)     |
+------------------------------------+
```

### Actuator module (per shutter, printed)
MG90D + multi-hole horn + M2×50 pushrod + multi-hole shutter clamp arm, mounted at the frame edge,
target footprint roughly **30 × 20 × 15 mm** of visible mechanism, hidden behind side trim where
possible.

---

## 14. Future Expansion

- **I2C header** reserved (3V3 / GND / SDA / SCL) for extra sensors, current monitor, OLED.
- **Remote WiFi actuator nodes** for *other rooms* later — the per-shutter MQTT/HomeSpan model and
  config array already accommodate this without re-architecting the hub (deferred; see
  [ADR 0001](decisions/0001-hub-vs-independent-nodes.md)).

---

## Final Product Definition

**ESP32 Smart Shutter Hub** — a modular controller driving a configurable number of MG90D
servo-operated shutter tilt mechanisms.

✅ Single ESP32 hub · N servo outputs (initially 4) via PCA9685 ·
✅ MG90D + M2 ball-link tilt actuation · ✅ per-shutter calibration + named favourites ·
✅ Apple HomeKit (HomeSpan bridge) · ✅ HA MQTT covers + sensors · ✅ single local web UI + OTA ·
✅ VEML7700 solar heat protection · ✅ manual-override handling · ✅ expandable to more shutters/rooms.

See [project-plan.md](project-plan.md) for the phased build roadmap and
[inventory.md](inventory.md) for the bill of materials.
