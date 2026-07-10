# Pinout Summary

Every GPIO the hub uses, per board. The **ESP32-D** table is authoritative — it is what the firmware
actually defaults to today. The **ESP32-C3** table is a *proposal*: the C3 build variants compile,
but their pin map has never been revisited and a hardware limit still blocks them. See
[C3 status](#esp32-c3-status--not-ready) before trusting it.

On the C3 the light sensor **shares the PCA9685's I²C bus** — it has only one I²C controller, so the
dedicated-bus default is clamped to shared ([ADR 0012](decisions/0012-selectable-sensor-i2c-bus.md)).

All I²C and servo pins are **configurable at runtime** (web UI → Servo test / Solar) and persisted in
NVS. The values below are the compiled-in defaults a factory-reset device comes up with.

---

## ESP32-D (ESP32-WROOM DevKit) — the locked, shipping board

PlatformIO `board = esp32dev`; variants `esp32d-pca9685` (the real hub) and `esp32d-direct` (a
single-servo bench build).

| GPIO | Function | Used by | Default set in |
| ---- | -------- | ------- | -------------- |
| **21** | I²C **SDA** — `Wire`, servo bus | PCA9685 @ `0x40` | `AppConfig.cpp` `g_i2cSda` |
| **22** | I²C **SCL** — `Wire`, servo bus | PCA9685 @ `0x40` | `AppConfig.cpp` `g_i2cScl` |
| **25** | I²C **SDA** — `Wire1`, sensor bus | VEML7700 @ `0x10` | `AppConfig.cpp` `g_lsSda` |
| **26** | I²C **SCL** — `Wire1`, sensor bus | VEML7700 @ `0x10` | `AppConfig.cpp` `g_lsScl` |
| **13** | Servo signal (**`-direct` builds only**) | one MG90D, LEDC-backed | `AppConfig.cpp` `g_servoPin` |
| 0 | Boot / recovery strapping | bootloader | — |
| 34 | *(optional, not implemented)* servo-rail voltage monitor | ADC1 | — |

Servos on a PCA9685 build hang off the **driver**, not the ESP32: **CH0–CH3 → shutters 1–4**, assigned
per shutter on the web UI's **Shutters** page.

**Two I²C buses, by default.** The PCA9685 (`0x40`) and VEML7700 (`0x10`) have distinct addresses
and *could* share one bus. By default they don't: the sensor gets its own `Wire1` so a damaged sensor
lead — a long run to a window — can never wedge the servo bus and strand the shutters
([ADR 0011](decisions/0011-dedicated-sensor-i2c-bus.md)). The bus is selectable on the **Solar** page
([ADR 0012](decisions/0012-selectable-sensor-i2c-bus.md)); in **shared** mode the sensor uses GPIO21/22
and these two pins are free.

### Pins the firmware will refuse

`ServoController::validGpio()` accepts only:

```
0 1 2 3 4 5 12 13 14 15 16 17 18 19 21 22 23 25 26 27 32 33
```

Excluded, and why:

| GPIO | Why it is rejected |
| ---- | ------------------ |
| 6–11 | Wired to the SPI flash chip — using them bricks the boot |
| 34–39 | **Input-only.** They cannot drive a servo signal or an open-drain I²C line |
| 20, 24, 28–31 | Not bonded out on the WROOM module |

The same whitelist validates the servo signal pin *and* the I²C pins, because I²C needs
bidirectional-capable pins. This is why the Solar page warns you off GPIO34–39.

---

## ESP32-C3 (ESP32-C3-DevKitM-1) — proposed, deferred

PlatformIO `board = esp32-c3-devkitm-1`; variants `esp32c3-direct`, `esp32c3-pca9685`. Not in
`default_envs`.

The C3 is a different chip with a different pinout — only **GPIO0–21 exist**, and several are spoken
for. A workable map *would* be:

| GPIO | Proposed function | Note |
| ---- | ----------------- | ---- |
| **8** | I²C SDA — servo bus (PCA9685) | Arduino core's default `SDA` on this board; also a strapping pin |
| **9** | I²C SCL — servo bus (PCA9685) | Arduino core's default `SCL`; also the BOOT strapping pin |
| **3** | Servo signal (`-direct` builds) | ADC1 capable; otherwise unencumbered |
| 8 / 9 | I²C sensor bus — **shared** with the PCA9685 | No `Wire1` on this chip; the VEML7700 (`0x10`) rides the servo bus alongside the PCA9685 (`0x40`) |

Reserved on the C3, do not use:

| GPIO | Reserved for |
| ---- | ------------ |
| 11 | `VDD_SPI` |
| 12–17 | In-package SPI flash |
| 18, 19 | USB-Serial-JTAG (D− / D+) — the programming and console port |
| 20, 21 | UART0 RX / TX |
| 2, 8, 9 | Strapping pins — must be left in a sane state at boot |

That leaves roughly **GPIO0–7 and 10** genuinely free.

### ESP32-C3 status — not ready

All four envs compile clean today (`esp32c3-pca9685` links at 89.5 % flash, `esp32c3-direct` at
90.1 %). One of the two blockers below is **fixed as of v0.6.1**; the other still stands.

1. ~~**Solar heat protection cannot work on the C3.**~~ **Fixed in v0.6.1**
   ([ADR 0012](decisions/0012-selectable-sensor-i2c-bus.md)). A dedicated sensor bus needs a second
   I²C controller — the ESP32-D has two (`SOC_I2C_NUM == 2`), the C3 has one. The Arduino core
   declares `Wire1` unconditionally, so the C3 build linked fine and then failed *silently* at
   runtime (`i2cInit(1,…)` → `ESP_ERR_INVALID_ARG`), showing a permanent "not detected".
   The sensor bus is now a **setting**: on a one-controller chip the dedicated preference is clamped
   to **shared** (the VEML7700 rides the PCA9685's `Wire`), and the Solar page disables the dedicated
   option and says why. Solar works on the C3 — at the cost of the fault isolation ADR 0011 bought.
2. **The GPIO whitelist is still ESP32-D's.** `validGpio()` hard-codes the WROOM's valid pins. On a
   C3 it would happily accept GPIO25/26/27/32/33 — **which do not exist** — and GPIO12–17, which are
   the flash. Nothing stops a user configuring a pin that bricks the boot. **This is why the C3 is
   still not shippable**, and why no C3 binaries are attached to releases.

Also worth knowing: the C3 has **6 LEDC channels** to the ESP32-D's 16, capping a `-direct` build's
servo count; and at ~90 % flash there is little headroom left for HomeSpan to grow into.

Until those are addressed, treat `esp32c3-*` bins as **untested engineering builds**, not releases.

---

## Changing the defaults

Nothing here needs a rebuild. The web UI writes all of it to NVS, where it survives OTA:

| Pins | Where |
| ---- | ----- |
| Servo-bus SDA / SCL (PCA9685) | **Servo test** page |
| Servo signal GPIO (`-direct`) | **Servo test** page |
| Sensor-bus SDA / SCL (VEML7700) | **Solar** page → Sensor card |
| PCA9685 channel per shutter | **Shutters** page |

**System → Quick Actions → Reset config** returns every one of them to the compiled-in defaults in
the tables above.
