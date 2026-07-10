# Inventory & Bill of Materials

Starting-state facts and parts the build assumes. Status: **⬤ ordered/in hand · ○ to source**.

## Shutters (existing — the thing being automated)

| Fact | Value |
| ---- | ----- |
| Panels | 4 (Front Room: Left, Left Centre, Right Centre, Right) |
| Panel width | ~450 mm |
| Slats per panel | 11 |
| Slat height | ~75 mm (3") |
| Material | lightweight wood or PVC blades, plastic tilt linkage |
| Tilt mechanism | one vertical tilt rod at the frame edge, moves all 11 slats |
| Replacing | Zemismart JM36/JC601 all-in-one Zigbee/battery tilt driver |
| Expected tilt-rod force | < 1 kg (to confirm — Phase 0 force test) |

## Electronics

| Part | Role | Status | Notes |
| ---- | ---- | ------ | ----- |
| ESP32-D DevKit | Hub controller | ⬤ (existing stock) | 16 LEDC channels; two I2C controllers ([pinout.md](pinout.md)) |
| PCA9685 | 16-ch servo driver | ⬤ | I2C 0x40 on `Wire` — **SDA GPIO21 / SCL GPIO22**; servos on CH0–3 |
| MG90D digital metal-gear servo ×4 | Tilt actuators | ⬤ | ~2 kg·cm, ~13–14 g, 4.8–6 V |
| VEML7700 | Ambient light sensor | ⬤ | I2C 0x10; own `Wire1` by default — **SDA GPIO25 / SCL GPIO26** — or shared with the PCA9685 ([ADR 0012](decisions/0012-selectable-sensor-i2c-bus.md)); 0–120,000 lux |
| USB-C PD charger (30 W+) | Mains source | ⬤ | |
| AITRIP PD trigger board | PD → 12 V | ⬤ | Set to 12 V |
| XL4015 buck converter (~5 A) | 12 V → **5.1 V** | ⬤ (ordered) | Replaces LM2596; large heatsink, screw terminals |
| 1000–2200 µF electrolytic (≥10 V) | Servo-rail bulk cap | ⬤ | Near PCA9685 V+ |
| LM2596 buck | (spare / superseded) | ⬤ | Kept as spare — see plan D4 |

## Mechanical / linkage

| Part | Role | Status | Notes |
| ---- | ---- | ------ | ----- |
| M2 × 50 mm ball-link pushrod | Servo → tilt rod | ⬤ | Link rod 50 mm; total adj. 75–85 mm; **hole-to-hole 68–78 mm**; 304 stainless + nylon ends |
| M2 × 60 mm ball-link pushrod | Backup length | ○ | A couple for mounting flexibility |
| Servo horns (8 / 10 / 12 mm holes) | Adjustable leverage | ○ | Start at 10 mm |
| 3D-printed shutter arm (holes 15/20/25 mm) | Adjustable leverage | ○ (to print) | Start at 20 mm |
| 3D-printed servo test bracket | Phase 0 proving | ○ (to print) | |
| Hub enclosure (printed) | Wall box, vented over XL4015, light window | ○ (to design) | |
| Actuator enclosures ×4 (printed) | Per-shutter modules | ○ (to design) | ~30×20×15 mm visible |

## Wiring

- Servo outputs: 3-wire (+5 V / GND / PWM) per channel.
- Connectors: grommet holes + strain relief (prototype) or JST-XH (tidier) — plan D2.
- **Common ground** across ESP32, PCA9685, and all servos.

## Power budget (worst case)

| Load | Current |
| ---- | ------- |
| ESP32-D | ~0.25 A |
| PCA9685 | ~0.05 A |
| 4 × MG90D (all moving) | 2–3 A peak |
| Headroom on XL4015 (~5 A) | ~1 A+ |
