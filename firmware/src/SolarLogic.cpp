#include "SolarLogic.h"
#include "AppConfig.h"
#include "LightSensor.h"
#include "Shutters.h"
#include "ServoController.h"
#include "Diagnostics.h"

namespace {
using namespace AppConfig;

constexpr uint32_t EVAL_MS       = 1000;                   // evaluate ~1 Hz (sensor cadence)
constexpr uint32_t SUSPEND_MS    = 2UL * 60UL * 60UL * 1000UL;  // manual override = 2 h

SolarLogic::State g_state   = SolarLogic::IDLE;
uint32_t g_phaseStart = 0;          // millis a COUNTING phase began
uint32_t g_lastEval   = 0;
uint32_t g_resumeAt[Shutters::MAX] = {0};   // per-shutter suspend deadline (millis); 0 = active

bool suspended(int i) {
  if (i < 0 || i >= Shutters::MAX) return false;
  uint32_t r = g_resumeAt[i];
  return r != 0 && (int32_t)(millis() - r) < 0;   // signed compare survives millis() wrap
}

// Pulse width for a shutter index + target preset, or Shutters::UNSET if not set.
int targetUs(int i, uint8_t target) {
  String id = Shutters::idAt(i);
  switch (target) {
    case TGT_OPEN:     return Shutters::edgeUs(id, true);
    case TGT_CLOSED:   return Shutters::edgeUs(id, false);
    case TGT_DAYLIGHT: return Shutters::favUs(id, false);
    case TGT_PRIVACY:  return Shutters::favUs(id, true);
    default:           return Shutters::UNSET;    // TGT_NONE
  }
}

const char *targetName(uint8_t t) {
  switch (t) { case TGT_OPEN: return "open"; case TGT_CLOSED: return "closed";
    case TGT_DAYLIGHT: return "daylight"; case TGT_PRIVACY: return "privacy"; default: return "do-nothing"; }
}

// Drive every calibrated, non-suspended shutter to a target preset. TGT_NONE moves nothing.
void applyTarget(uint8_t target, const char *why) {
  if (target == TGT_NONE) { LOGI("solar", "%s → do nothing (leave slats put)", why); return; }
  int moved = 0, skipped = 0;
  for (int i = 0; i < Shutters::count(); i++) {
    if (!Shutters::calibratedAt(i)) { skipped++; continue; }
    if (suspended(i))               { skipped++; continue; }   // manual override in effect
    int us = targetUs(i, target);
    if (us == Shutters::UNSET)      { skipped++; continue; }    // that preset not saved
    uint8_t slot = ServoController::usesPca() ? (uint8_t)Shutters::channelAt(i) : 0;
    ServoController::moveSlotUs(slot, us);
    moved++;
  }
  LOGI("solar", "%s → %s: moved %d, skipped %d", why, targetName(target), moved, skipped);
}

void enter(SolarLogic::State s) { g_state = s; g_phaseStart = millis(); }
}  // namespace

namespace SolarLogic {

void begin() {
  g_state = IDLE;
  for (int i = 0; i < Shutters::MAX; i++) g_resumeAt[i] = 0;
  LOGI("solar", "ready — %s", AppConfig::solarEnabled() ? "automation on" : "automation off");
}

void loop() {
  uint32_t now = millis();
  if (now - g_lastEval < EVAL_MS) return;
  g_lastEval = now;

  if (!AppConfig::solarEnabled()) { if (g_state != IDLE) enter(IDLE); return; }

  float    lux      = LightSensor::lux();
  uint32_t tripLux  = AppConfig::solarTripLux();
  uint32_t clearLux = AppConfig::solarClearLux();
  uint32_t tripMs   = (uint32_t)AppConfig::solarTripSecs()  * 1000UL;
  uint32_t clearMs  = (uint32_t)AppConfig::solarClearSecs() * 1000UL;

  switch (g_state) {
    case IDLE:
      if (lux >= tripLux) { enter(COUNT_TRIP); LOGD("solar", "bright (%.0f lx) — counting to trip", lux); }
      break;
    case COUNT_TRIP:
      if (lux < tripLux) { enter(IDLE); LOGD("solar", "fell back below trip — cancelled"); }
      else if (now - g_phaseStart >= tripMs) { applyTarget(AppConfig::solarBrightTarget(), "trip"); enter(TRIPPED); }
      break;
    case TRIPPED:
      if (lux <= clearLux) { enter(COUNT_CLEAR); LOGD("solar", "dimmed (%.0f lx) — counting to clear", lux); }
      break;
    case COUNT_CLEAR:
      if (lux > clearLux) { enter(TRIPPED); LOGD("solar", "brightened above clear — cancelled"); }
      else if (now - g_phaseStart >= clearMs) { applyTarget(AppConfig::solarClearTarget(), "clear"); enter(IDLE); }
      break;
  }
}

State state() { return g_state; }

const char *stateText() {
  if (!AppConfig::solarEnabled()) return "disabled";
  switch (g_state) {
    case COUNT_TRIP:  return "counting-trip";
    case TRIPPED:     return "tripped";
    case COUNT_CLEAR: return "counting-clear";
    default:          return "idle";
  }
}

uint32_t secondsRemaining() {
  if (g_state != COUNT_TRIP && g_state != COUNT_CLEAR) return 0;
  uint32_t dwellMs = (g_state == COUNT_TRIP ? AppConfig::solarTripSecs() : AppConfig::solarClearSecs()) * 1000UL;
  uint32_t elapsed = millis() - g_phaseStart;
  return elapsed >= dwellMs ? 0 : (dwellMs - elapsed) / 1000UL;
}

void notifyManualMove(const String &id) {
  int i = Shutters::find(id);
  if (i < 0 || i >= Shutters::MAX) return;
  g_resumeAt[i] = millis() + SUSPEND_MS;
  if (g_resumeAt[i] == 0) g_resumeAt[i] = 1;   // 0 is the "active" sentinel — never land on it
  LOGI("solar", "%s moved manually — automation suspended 2 h", id.c_str());
}

int suspendedCount() {
  int n = 0;
  for (int i = 0; i < Shutters::count(); i++) if (suspended(i)) n++;
  return n;
}

}  // namespace SolarLogic
