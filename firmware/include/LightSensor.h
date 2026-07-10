// LightSensor — VEML7700 ambient-light driver for solar heat protection (Phase 6, v0.6.0).
//
// The sensor lives on its OWN I2C bus (Wire1, pins from AppConfig, default SDA 25 / SCL 26)
// so a sensor-lead fault can never wedge the PCA9685 servo bus on Wire — see ADR 0011.
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
void  begin();          // read config; bring up Wire1 + configure the chip if enabled
void  loop();           // periodic read (~1 Hz); no-op until enabled
void  reconfigure();    // re-init the bus/chip after the pins or enable flag change

bool  enabled();        // sensor configured on (AppConfig::lsEnabled)
bool  present();        // the chip ACKed at 0x10 on the last probe (false in simulate-only)
float lux();            // latest effective lux — the simulated value if an override is set,
                        // else the live reading (0 when disabled/absent)
bool  simulated();      // true while a simulate override is active
void  simulate(float lux);   // force an effective lux (test without hardware)
void  useLive();        // drop the override, return to the live reading
}
