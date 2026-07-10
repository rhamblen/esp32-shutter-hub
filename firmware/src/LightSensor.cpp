#include "LightSensor.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include <Wire.h>

// VEML7700 on Wire1. Register map (each command is a 16-bit little-endian word):
//   0x00 ALS_CONF_0   config: gain [12:11], integration time [9:6], ALS_SD (shutdown) [0]
//   0x04 ALS          ambient-light counts (what we read)
// Config we write: gain x1/8 (0b10 << 11) + IT 25 ms (0b1100 << 6), sensor powered on.
// That is the least-sensitive setting — deliberate, so the top of the range reaches full
// sun (~120 k lx) instead of saturating at a few thousand. Resolution at this setting is
// 1.8432 lx/count (Vishay app note). We report lux = counts * resolution WITHOUT the
// vendor's 4th-order high-lux correction: that polynomial diverges wildly across the
// 60–120 k lx band this sensor is here to watch, and absolute accuracy is irrelevant to a
// user-calibrated trip threshold. Monotonic and stable is what matters.
namespace {
constexpr uint8_t  ADDR       = 0x10;
constexpr uint8_t  REG_CONF   = 0x00;
constexpr uint8_t  REG_ALS    = 0x04;
constexpr uint16_t CONF_SUN   = 0x1300;    // gain 1/8, IT 25 ms, powered on (ALS_SD = 0)
constexpr float    RES_LX_CT  = 1.8432f;   // lx per count at gain 1/8 + IT 25 ms
constexpr uint32_t READ_MS    = 1000;      // sample cadence

TwoWire &bus = Wire1;                       // dedicated bus — never the servo bus (Wire)
bool     g_present = false;
float    g_live    = 0.0f;
bool     g_sim     = false;
float    g_simLux  = 0.0f;
uint32_t g_lastRead = 0;

bool writeReg(uint8_t reg, uint16_t val) {
  bus.beginTransmission(ADDR);
  bus.write(reg);
  bus.write((uint8_t)(val & 0xFF));
  bus.write((uint8_t)(val >> 8));
  return bus.endTransmission() == 0;
}

bool readReg(uint8_t reg, uint16_t &out) {
  bus.beginTransmission(ADDR);
  bus.write(reg);
  if (bus.endTransmission(false) != 0) return false;        // repeated start
  if (bus.requestFrom((int)ADDR, 2) != 2) return false;
  uint16_t lo = bus.read(), hi = bus.read();
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

void begin() {
  if (!AppConfig::lsEnabled()) { LOGI("light", "sensor disabled"); return; }
  if (AppConfig::lsType() != 0) { LOGW("light", "analog sensor type not implemented — treating as disabled"); return; }
  uint8_t sda = AppConfig::lsSda(), scl = AppConfig::lsScl();
  bus.begin(sda, scl);
  LOGI("light", "Wire1 up on SDA %u / SCL %u", sda, scl);
  if (!configure()) LOGW("light", "VEML7700 not detected @0x%02X — check wiring/pins", ADDR);
}

void loop() {
  if (!AppConfig::lsEnabled() || AppConfig::lsType() != 0) return;
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
  bus.end();
  begin();
}

bool  enabled()   { return AppConfig::lsEnabled() && AppConfig::lsType() == 0; }
bool  present()   { return g_present; }
bool  simulated() { return g_sim; }
float lux()       { return g_sim ? g_simLux : (enabled() ? g_live : 0.0f); }

void simulate(float lux) { g_sim = true; g_simLux = lux < 0 ? 0 : lux; }
void useLive()           { g_sim = false; }

}  // namespace LightSensor
