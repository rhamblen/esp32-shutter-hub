// LightSensor — VEML7700 ambient-light driver for solar heat protection (Phase 6, v0.6.0).
//
// The I2C bus is a **setting**, not a constant (v0.6.1, ADR 0012):
//   BUS_DEDICATED  its own `Wire1` on AppConfig::lsSda/lsScl (default 25/26). The default:
//                  a sensor-lead fault can't wedge the PCA9685's bus (ADR 0011). Requires a
//                  SoC with two I2C controllers — the ESP32-D has two, the ESP32-C3 has one.
//   BUS_SHARED     rides the PCA9685's `Wire` on AppConfig::i2cSda/i2cScl (21/22). Always
//                  available, and the ONLY option on a one-controller chip.
// `dedicatedSupported()` reports the SoC capability; a dedicated preference on a chip that
// can't honour it is silently clamped to shared, so the feature degrades rather than dies.
//
// Lean vendored register driver (no Adafruit lib) to stay under the flash budget the
// HomeSpan build already pushes: fixed gain 1/8 + 25 ms integration → ~1.84 lx/count,
// full-scale ≈ 120 k lx (full sun). Reported lux is LINEAR/approximate (the Vishay
// high-lux correction is skipped — see the .cpp): fine for threshold-based tripping,
// which is calibrated against observed readings anyway.
//
// A simulate override lets the whole solar feature be built and tested before the
// physical sensor arrives (drive it from the Solar page slider).
#pragma once
#include <Arduino.h>

namespace LightSensor {
void  begin();          // read config; bring up the chosen bus + configure the chip if enabled
void  loop();           // periodic read (~1 Hz); no-op until enabled
void  reconfigure();    // re-init after the bus/pins/enable flag change (never tears down a shared bus)

bool  enabled();        // sensor configured on (AppConfig::lsEnabled, VEML7700 type)
bool  present();        // the chip ACKed at 0x10 on the last probe (false in simulate-only)

bool    dedicatedSupported();  // this SoC has a second I2C controller (false on the ESP32-C3)
uint8_t activeBus();           // the bus actually in use — AppConfig::SensorBus, after clamping
uint8_t activeSda();           // the pins actually in use (the PCA9685's when shared)
uint8_t activeScl();

float lux();            // latest effective lux — the simulated value if an override is set,
                        // else the live reading (0 when disabled/absent)
bool  simulated();      // true while a simulate override is active
void  simulate(float lux);   // force an effective lux (test without hardware)
void  useLive();        // drop the override, return to the live reading
}
