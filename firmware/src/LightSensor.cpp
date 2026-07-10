#include "LightSensor.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include <Wire.h>
#include <soc/soc_caps.h>

// VEML7700 register map (each command is a 16-bit little-endian word):
//   0x00 ALS_CONF_0   config: gain [12:11], integration time [9:6], ALS_SD (shutdown) [0]
//   0x04 ALS          ambient-light counts (what we read)
// Config we write: gain x1/8 (0b10 << 11) + IT 25 ms (0b1100 << 6), sensor powered on.
// That is the least-sensitive setting — deliberate, so the top of the range reaches full
// sun (~120 k lx) instead of saturating at a few thousand. Resolution at this setting is
// 1.8432 lx/count (Vishay app note). We report lux = counts * resolution WITHOUT the
// vendor's 4th-order high-lux correction: that polynomial diverges wildly across the
// 60–120 k lx band this sensor is here to watch, and absolute accuracy is irrelevant to a
// user-calibrated trip threshold. Monotonic and stable is what matters.
//
// BUS OWNERSHIP (ADR 0012). `TwoWire::begin()` is idempotent on an already-started master
// bus: the Arduino core logs "Bus already started in Master Mode." and returns true without
// re-initialising or changing pins. So in shared mode we can call begin() unconditionally —
// on a PCA9685 build ServoController::begin() (which runs first in main.cpp) already brought
// `Wire` up and ours is a harmless no-op; on a -direct build nothing had, and ours is the
// call that initialises it. First caller wins; no ownership dance needed.
//
// The mirror image is NOT safe: `end()` really does tear the bus down. We must never call it
// on `Wire` — that would kill the PCA9685 and freeze every servo. Only the dedicated `Wire1`
// is ever ended, tracked by g_dedUp.
namespace {
constexpr uint8_t  ADDR       = 0x10;
constexpr uint8_t  REG_CONF   = 0x00;
constexpr uint8_t  REG_ALS    = 0x04;
constexpr uint16_t CONF_SUN   = 0x1300;    // gain 1/8, IT 25 ms, powered on (ALS_SD = 0)
constexpr float    RES_LX_CT  = 1.8432f;   // lx per count at gain 1/8 + IT 25 ms
constexpr uint32_t READ_MS    = 1000;      // sample cadence

TwoWire *g_bus     = nullptr;   // whichever bus we ended up on (never dereferenced until begin())
bool     g_dedUp   = false;     // we brought Wire1 up and therefore may tear it down
bool     g_present = false;
float    g_live    = 0.0f;
bool     g_sim     = false;
float    g_simLux  = 0.0f;
uint32_t g_lastRead = 0;

// A dedicated bus needs a second I2C controller. The SoC header knows — no board whitelist
// to rot. ESP32-D: 2. ESP32-C3: 1.
constexpr bool HAS_SECOND_I2C = SOC_I2C_NUM > 1;

// The bus we will actually use: the preference, clamped to shared when the SoC can't do better.
uint8_t effectiveBus() {
  uint8_t pref = AppConfig::lsBus();
  if (pref == AppConfig::BUS_DEDICATED && !HAS_SECOND_I2C) return AppConfig::BUS_SHARED;
  return pref;
}

bool writeReg(uint8_t reg, uint16_t val) {
  if (!g_bus) return false;
  g_bus->beginTransmission(ADDR);
  g_bus->write(reg);
  g_bus->write((uint8_t)(val & 0xFF));
  g_bus->write((uint8_t)(val >> 8));
  return g_bus->endTransmission() == 0;
}

bool readReg(uint8_t reg, uint16_t &out) {
  if (!g_bus) return false;
  g_bus->beginTransmission(ADDR);
  g_bus->write(reg);
  if (g_bus->endTransmission(false) != 0) return false;      // repeated start
  if (g_bus->requestFrom((int)ADDR, 2) != 2) return false;
  uint16_t lo = g_bus->read(), hi = g_bus->read();
  out = lo | (hi << 8);
  return true;
}

// Probe + configure. Sets g_present. Safe to re-run (used by reconfigure() and as a
// retry when the sensor is plugged in after boot).
bool configure() {
  g_present = writeReg(REG_CONF, CONF_SUN);
  if (g_present) LOGI("light", "VEML7700 found @0x%02X (gain 1/8, IT 25ms)", ADDR);
  return g_present;
}
}  // namespace

namespace LightSensor {

bool dedicatedSupported() { return HAS_SECOND_I2C; }
uint8_t activeBus() { return effectiveBus(); }

uint8_t activeSda() {
  return effectiveBus() == AppConfig::BUS_DEDICATED ? AppConfig::lsSda() : AppConfig::i2cSda();
}
uint8_t activeScl() {
  return effectiveBus() == AppConfig::BUS_DEDICATED ? AppConfig::lsScl() : AppConfig::i2cScl();
}

void begin() {
  g_present = false;
  if (!AppConfig::lsEnabled()) { LOGI("light", "sensor disabled"); return; }
  if (AppConfig::lsType() != 0) { LOGW("light", "analog sensor type not implemented — treating as disabled"); return; }

  bool ded = effectiveBus() == AppConfig::BUS_DEDICATED;
  if (!ded && AppConfig::lsBus() == AppConfig::BUS_DEDICATED)
    LOGW("light", "this chip has one I2C controller — falling back to the shared bus");

  uint8_t sda = activeSda(), scl = activeScl();
  g_bus = ded ? &Wire1 : &Wire;
  // Idempotent when the bus is already up (shared, PCA9685 build) — see the note above.
  if (!g_bus->begin(sda, scl)) {
    LOGW("light", "I2C begin failed on %s (SDA %u / SCL %u)", ded ? "Wire1" : "Wire", sda, scl);
    g_bus = nullptr;
    return;
  }
  g_dedUp = ded;
  LOGI("light", "%s bus on SDA %u / SCL %u", ded ? "dedicated (Wire1)" : "shared (Wire)", sda, scl);
  if (!configure()) LOGW("light", "VEML7700 not detected @0x%02X — check wiring/pins", ADDR);
}

void loop() {
  if (!enabled() || !g_bus) return;
  uint32_t now = millis();
  if (now - g_lastRead < READ_MS) return;
  g_lastRead = now;
  if (!g_present) { configure(); return; }               // retry a plugged-in-late sensor
  uint16_t raw;
  if (!readReg(REG_ALS, raw)) { g_present = false; LOGW("light", "read failed — will re-probe"); return; }
  g_live = raw * RES_LX_CT;
}

void reconfigure() {
  g_present = false;
  // Only ever tear down the bus we own. Ending `Wire` would take the PCA9685 with it.
  if (g_dedUp) { Wire1.end(); g_dedUp = false; }
  g_bus = nullptr;
  begin();
}

bool  enabled()   { return AppConfig::lsEnabled() && AppConfig::lsType() == 0; }
bool  present()   { return g_present; }
bool  simulated() { return g_sim; }
float lux()       { return g_sim ? g_simLux : (enabled() ? g_live : 0.0f); }

void simulate(float lux) { g_sim = true; g_simLux = lux < 0 ? 0 : lux; }
void useLive()           { g_sim = false; }

}  // namespace LightSensor
