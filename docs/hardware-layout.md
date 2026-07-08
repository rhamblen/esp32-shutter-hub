# Hardware Layout — hub prototype on solderable copper breadboard

Physical build plan for the hub: what sits on the copper breadboard, what is raised on nylon
standoffs, where the track cuts/bridges go, and which connectors to use. Companion to
[architecture.md](architecture.md) (topology) and [inventory.md](inventory.md) (BOM).

## Assumptions

- **Board:** solderable copper breadboard in the classic 400-point pattern — 30 columns of two
  5-hole strips (`a–e` / `f–j`) with a centre gap, plus a red/blue rail pair top **and** bottom
  (ElectroCookie / SB-400 style). One board builds the whole hub.
- **ESP32:** DevKit on **female pin-socket headers** (its "standoffs") so the module plugs in and
  can be swapped. Column counts below are for the 38-pin DevKitC (19 pins/side); a 30-pin DevKit
  V1 is 15 pins/side — same plan, 4 columns shorter.
- **Modules with mounting holes** (PCA9685, XL4015, PD trigger, VEML7700) are **raised on nylon
  standoffs**, not soldered flat — serviceable, and the XL4015 needs the airflow.

## The one rule that shapes everything

> **Servo current (2–3 A peak) never touches the breadboard.**
> Breadboard strips are thin copper good for ~0.5–1 A. The 5.1 V *servo* path runs point-to-point
> in 18 AWG: XL4015 OUT → PCA9685 **V+ screw terminal** (bulk cap there). The breadboard carries
> only the ESP32 + logic (~300 mA), fed by its own 5.1 V spur.

---

## Diagram 1 — system overview: on-board vs raised

```
 MAINS ══ USB-C PD charger (30 W+)
              │ USB-C cable
              ▼
 ┌─ raised on standoffs (power chain — never on the breadboard) ─────────────────┐
 │                                                                               │
 │  AITRIP PD trigger ──12 V──▶ XL4015 buck (set 5.1 V FIRST)                    │
 │   (enclosure wall,           (hot: vented, clear air above)                   │
 │    M2.5 standoffs)                │            │                              │
 │                        5.1 V ────┤            └──── 5.1 V ──┐  18 AWG        │
 │                        18 AWG    │                          ▼                 │
 │                                  │            PCA9685 V+ screw term           │
 │                                  │            + 1000–2200 µF bulk cap         │
 │                                  │            (M2.5 standoffs)                │
 │                                  │                 │ CH0–CH3 0.1" headers     │
 │                                  │                 ▼                          │
 │                                  │            MG90D ×4 (servo loom)           │
 └──────────────────────────────────┼─────────────────────────────────────────── ┘
                                    │ 5.1 V logic spur (screw terminal J1)
                                    ▼
 ┌─ ON the copper breadboard ─────────────────────────────────────────────────┐
 │  ESP32 DevKit (socketed) · rail jumpers · GPIO34 divider (optional)        │
 │  J2 → PCA9685 logic (3V3 / GND / SDA / SCL / OE)                           │
 │  J3 → VEML7700 remote cable (3V3 / GND / SDA / SCL)                        │
 └────────────────────────────────────────────────────────────────────────────┘
 Common ground: single point at the XL4015 OUT− terminal.
```

---

## Diagram 2 — hub baseboard, plan view

Columns numbered 1–30 left to right. ESP32 USB port faces right (programming access); the
**antenna end overhangs the left board edge** — never sit the antenna over copper.

```
          1   2   3   4   5 … … … … … … … … 20  21  22  23  24  25  26  27  28  29  30
        ┌───────────────────────────────────────────────────────────────────────────────┐
 5.1 V  │ + ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━◀━┓        │
 GND    │ − ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━◀━┓ ┃        │
        │                                                                    ┏┻━┻┓      │
        │      a ○ ○ ○ ○ … breakout wires from strips b–e … ○ ○      ○ ○ ○  ┃ J1 ┃      │
    ◀━━━┿━━┓  ┌────────────────────────────────────────┐                    ┃scrw┃      │
 antenna│  ┃  │      ESP32 DevKit  (socketed on        │    R1┌──┐          ┗━━━━┛      │
 hangs  │  ┗━━│      female headers, cols 2–20)   [USB]│═▶    │  │100k   ┌─────┐┌─────┐ │
 off the│      │  pins → rows a & j; strips b–e/f–i    │    R2│  │100k   │ J2  ││ J3  │ │
 edge   │      │  are the fan-out under the module     │      └──┘+100n  │XH-5 ││XH-4 │ │
        │      └────────────────────────────────────────┘   GPIO34 div.  │PCA  ││VEML │ │
        │      f ○ ○ ○ ○ … ○ ○                             (cols 22–24)  └─────┘└─────┘ │
        │                                                                (cols 25–30)   │
 3V3    │ + ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  │
 GND    │ − ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  │
        └───────────────────────────────────────────────────────────────────────────────┘
```

| Ref | Part | Position | Notes |
| --- | ---- | -------- | ----- |
| — | 2 × 19-pin female headers | cols 2–20, rows a & j | ESP32 socket. 30-pin DevKit: 15-pin headers, cols 2–16 |
| J1 | 2-pin 5.08 mm screw terminal | cols 28–30, top edge | 5.1 V logic spur in from XL4015 |
| J2 | JST-XH 5-pin | cols 25–27, row f–j side | PCA9685 logic: 3V3 · GND · SDA · SCL · OE |
| J3 | JST-XH 4-pin | cols 28–30, row f–j side | VEML7700 cable: 3V3 · GND · SDA · SCL |
| R1 R2 + C | 2 × 100 kΩ + 100 nF | cols 22–24 | optional GPIO34 servo-rail divider (plan D5) |
| C1 | 470 µF + 100 nF | at J1, across 5.1 V/GND rails | local decoupling (bulk cap lives at PCA9685) |

**Wiring the strips under the ESP32:** because a DevKit is wide, its pins land in rows **a** and
**j** and the module covers the rest of each strip. On a *solderable* board that's fine — the
remaining holes (b–e / f–i) of each pin column are your fan-out — but **solder every under-module
jumper and the socket headers before plugging the ESP32 in**, and route the rest on the underside.

Jumpers to fit (short solid-core on top, or bus wire underneath):

| From strip (ESP32 pin) | To |
| ---------------------- | -- |
| VIN | top **5.1 V** rail |
| GND (both) | top **GND** rail |
| 3V3 | bottom **3V3** rail |
| GPIO21 (SDA) | J2 pin 3 and J3 pin 3 |
| GPIO22 (SCL) | J2 pin 4 and J3 pin 4 |
| GPIO34 | divider mid-point (only if D5 populated) |
| bottom 3V3 / GND rails | J2 pins 1–2, J3 pins 1–2 |

---

## Diagram 3 — rail plan, cuts and bridges

The two rail pairs get **different voltages** — that's the point of having both:

```
 TOP    red  ━━━━━━━━━━━━━━━━━━━━━━━━━━  5.1 V   (from J1; feeds ESP32 VIN only)
 TOP    blue ━━━━━━━━━━━━━━━━━━━━━━━━━━  GND
        ~~~~~~~~~~~~ component field ~~~~~~~~~~~~
 BOTTOM red  ━━━━━━━━━━━━━━━━━━━━━━━━━━  3V3     (from ESP32 3V3 pin; feeds I2C devices)
 BOTTOM blue ━━━━━━━━━━━━━━━━━━━━━━━━━━  GND     (jumper to top GND rail once, near J1)

 CHECK before soldering (meter on continuity):
 ✂ CUT  — if your board ties top and bottom red rails together at either end
           (some copper breadboards do), cut that link: top is 5.1 V, bottom is 3V3.
 ⌒ BRIDGE — if the rails are split at the mid-point (many boards copy the solderless
           break at column 15), solder-bridge all four splits — we want full-length rails.
 ✂ (none) — no cuts are needed in the a–e / f–j field: each ESP32 pin owns its column strip.
```

Running the PCA9685's logic (`VCC`) and the VEML7700 from the **3V3 rail** keeps the I2C bus at
3.3 V — no level shifting, and the VEML7700 is 3.3 V-only anyway. Servo power enters the PCA9685
separately at its V+ screw terminal.

---

## Diagram 4 — raised modules: standoffs and drill plan

```
   baseplate / enclosure floor  (side view)
   ═══════════════════════════════════════════════════════════════════
    ▲ M3×10 nylon    ▲ M2.5×12 nylon      ▲ M3×10 nylon    ▲ M2.5×10
   ┌┴────────────┐  ┌┴─────────────┐     ┌┴───────────┐   ┌┴─────────┐
   │ copper      │  │ PCA9685      │     │ XL4015     │   │ PD       │
   │ breadboard  │  │ + bulk cap   │     │ (leave     │   │ trigger  │
   │ ┌─────────┐ │  │ on V+ term   │     │  ≥20 mm    │   │ (USB-C   │
   │ │ ESP32 on│ │  │ servo hdrs → │     │  air above,│   │  faces   │
   │ │ socket  │ │  │ face servos  │     │  vent slot)│   │  wall    │
   │ └─────────┘ │  └──────────────┘     └────────────┘   │  cutout) │
   └─────────────┘                                        └──────────┘
```

| Module | Holes | Standoff | Placement |
| ------ | ----- | -------- | --------- |
| Copper breadboard | M3 corner holes (most copper boards have them) | M3 × 10 nylon | anywhere; ESP32 USB reachable |
| PCA9685 | 4 × M2.5 | M2.5 × 12 nylon | servo headers facing the loom exit; bulk cap at V+ |
| XL4015 | 2 × M3 | M3 × 10 nylon | ≥ 20 mm clearance above heatsink; under the enclosure vent |
| AITRIP PD trigger | 2 × M2.5 (varies) | M2.5 × 10 nylon | USB-C at the enclosure wall cutout |
| VEML7700 | 2 × M2.5 | M2.5 × 6 nylon | behind the enclosure light window, on the J3 cable |

**Mounting options** — either works, pick per module:

1. **Beside the board (preferred):** standoffs screw into holes drilled in the baseplate /
   enclosure floor. Everything stays serviceable and the breadboard stays untouched.
2. **On the board:** a copper breadboard *can* be drilled for a standoff — use an unused column
   region (e.g. cols 21–30 row f–j if J2/J3 move), drill M3 through, deburr both faces, and use
   **nylon screws + standoffs only** (the strips are live copper; metal hardware would short
   adjacent strips). Confirm with the meter that no wanted strip was cut by the hole.

---

## Diagram 5 — cable loom and connectors

```
                          ┌────────────┐  4-core, ~0.3–0.5 m
  breadboard J3 (XH-4) ───┤ 3V3 GND    ├────────────────────▶ VEML7700 @ light window
                          │ SDA SCL    │
                          └────────────┘
                          ┌────────────┐  5-core, short (~0.15 m)
  breadboard J2 (XH-5) ───┤ 3V3 GND    ├────────────────────▶ PCA9685 logic pins
                          │ SDA SCL OE │
                          └────────────┘
  XL4015 OUT+ / OUT− ══ 18 AWG pair ══▶ PCA9685 V+ / GND screw terminals (+ bulk cap)
  XL4015 OUT+ / OUT− ── 22 AWG pair ──▶ breadboard J1 (logic spur)
  PD trigger 12 V     ══ 18 AWG pair ══▶ XL4015 IN+ / IN−

  PCA9685 CH0–CH3 ──▶ servo's own 3-pin 0.1" plug ──▶ extension lead ──▶ grommet ──▶ MG90D
                       (orange=PWM · red=V+ · brown=GND — brown towards the board edge)
```

### Connector recommendations (from "the different connectors I can use")

| Connection | Use | Why |
| ---------- | --- | --- |
| 12 V and 5.1 V power runs | **screw terminals** (on-module) + ferruled 18 AWG | current-rated, re-tightenable, no crimp needed |
| 5.1 V logic spur onto breadboard | **2-pin 5.08 mm screw terminal (J1)** | matches the power-chain style; won't pull out |
| I2C / logic cables (J2, J3) | **JST-XH 2.54 mm** | polarised — can't reverse 3V3/GND; latched, serviceable |
| Servos → PCA9685 | **native 0.1" servo plugs** straight onto CH0–3 | the PCA9685 header *is* a servo header; zero adapters |
| Servo loom → wall (prototype) | grommet holes + strain relief | plan D2 prototype option; revisit JST-XH panel breakout at Phase 7 |
| Anything Dupont | avoid for power; OK for bench probing only | unlatched Dupont on a servo rail is how brownouts happen |

**Don't** run 5 V or servo current through JST-XH shells rated 3 A only at a pinch, and don't use
unpolarised Dupont pairs for anything permanent — one reversed 3V3/GND on the VEML7700 kills it.

---

## Build order

1. Bench-set the **XL4015 to 5.1 V** with a meter, unloaded, before anything else.
2. Breadboard first pass: meter the rails, do the ✂ cut / ⌒ bridge checks from Diagram 3.
3. Solder under-ESP32 jumpers and strip fan-out wires, then the female headers, then J1/J2/J3,
   divider, and decoupling. Continuity-check every net **before** plugging the ESP32 in.
4. Mount modules on standoffs; run the 18 AWG power pairs; fit the bulk cap at PCA9685 V+
   (mind polarity).
5. Power up with **no servos**: check 5.1 V at J1 and PCA9685 V+, 3V3 rail at J2/J3, then
   `i2cdetect`-style scan expects **0x40** (PCA9685) and **0x10** (VEML7700).
6. Add one MG90D on CH0 and re-run the Phase 1 bench test; watch for resets under load.
