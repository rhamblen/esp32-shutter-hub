#include "ServoController.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include <ESP32Servo.h>

// Single-servo driver. Position is tracked in MICROSECONDS (pulse width) — the
// servo's native unit — so Phase-2 blind calibration can land a slat far more
// finely than whole degrees. The Servo-test page still talks in degrees; those
// are derived from µs on the way in/out.
//
// Uses the ESP32Servo library (LEDC-backed). Stays DETACHED at boot so nothing
// twitches until the user acts — this also dodges the attach-time current surge
// on a bare/USB-powered board.
namespace {
Servo    g_servo;
uint8_t  g_pin      = 13;
float    g_curUs    = 1500;       // pulse width actually on the output right now
int      g_tgtUs    = 1500;       // pulse width we're slewing toward
uint16_t g_speed    = 25;         // max slew rate in deg/s; 0 = unlimited (snap)
bool     g_sweep    = false;
uint32_t g_lastTick = 0;

// MG90-class pulse envelope. Wider than the classic 1000–2000 µs so the endpoints
// reach the servo's real mechanical limits during calibration.
const int      MIN_US          = 500;
const int      MAX_US          = 2500;
const uint32_t MOVE_TICK_MS    = 20;   // one 50 Hz servo frame
const uint16_t SWEEP_DPS_UNCAP = 133;  // sweep rate used when speed is "Max"

// One degree spans this many µs across the 0–180° / MIN..MAX map — used to turn
// the deg/s speed limit into a µs/s slew rate.
const float US_PER_DEG = (float)(MAX_US - MIN_US) / 180.0f;

int   angleToUs(int a) { return map(constrain(a, 0, 180), 0, 180, MIN_US, MAX_US); }
int   usToAngle(int u) { return map(constrain(u, MIN_US, MAX_US), MIN_US, MAX_US, 0, 180); }
int   clampUs(int u)   { return constrain(u, MIN_US, MAX_US); }

void ensureAttached() {
  if (!g_servo.attached()) {
    g_servo.setPeriodHertz(50);            // standard analog-servo frame rate
    g_servo.attach(g_pin, MIN_US, MAX_US);
  }
}

// Point the servo at an absolute pulse width, honouring the speed limit.
void moveToUs(int us) {
  g_sweep = false;                         // a direct move cancels an in-progress sweep
  g_tgtUs = clampUs(us);
  ensureAttached();
  if (g_speed == 0) {                      // Max speed — snap straight there
    g_curUs = g_tgtUs;
    g_servo.writeMicroseconds(g_tgtUs);
  }                                        // otherwise loop() slews there at g_speed
}
}  // namespace

namespace ServoController {

void begin() {
  g_pin   = AppConfig::servoPin();
  g_speed = AppConfig::servoSpeedDps();
  // ESP32Servo shares the four LEDC hardware timers; hand them over up front.
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  LOGI("servo", "driver ready — signal GPIO%u, detached (idle)", g_pin);
}

bool isValidPin(uint8_t g) {
  // Output-capable GPIOs on the ESP32-WROOM (ESP32-D). Excludes input-only 34–39,
  // flash 6–11, and non-existent pins. Strapping pins (0/2/12/15) are allowed but
  // not ideal — the UI flags them.
  static const uint8_t ok[] = {0,1,2,3,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33};
  for (uint8_t p : ok) if (p == g) return true;
  return false;
}

uint8_t pin() { return g_pin; }

bool setPin(uint8_t g) {
  if (!isValidPin(g)) { LOGW("servo", "rejected GPIO%u (not output-capable)", g); return false; }
  bool wasAttached = g_servo.attached();
  g_sweep = false;
  if (wasAttached) g_servo.detach();
  g_pin = g;
  AppConfig::setServoPin(g);
  if (wasAttached) { ensureAttached(); g_servo.writeMicroseconds((int)roundf(g_curUs)); }
  LOGI("servo", "signal pin set to GPIO%u", g_pin);
  return true;
}

bool attach() {
  ensureAttached();
  g_tgtUs = (int)roundf(g_curUs);      // hold position — no surprise move on attach
  g_servo.writeMicroseconds(g_tgtUs);
  LOGI("servo", "attached on GPIO%u at %d µs", g_pin, g_tgtUs);
  return g_servo.attached();
}

void detach() {
  g_sweep = false;
  g_tgtUs = (int)roundf(g_curUs);      // freeze any in-flight move where it stopped
  if (g_servo.attached()) g_servo.detach();
  LOGI("servo", "detached (released)");
}

bool attached() { return g_servo.attached(); }

void writeAngle(int deg) { moveToUs(angleToUs(constrain(deg, 0, 180))); }
void writeUs(int us)     { moveToUs(us); }

void jogUs(int deltaUs) {
  // Nudge relative to the current target so repeated clicks accumulate cleanly,
  // even if a slew is still settling from the last one.
  moveToUs(g_tgtUs + deltaUs);
}

void run(int dir) {
  if      (dir > 0) moveToUs(MAX_US);          // slow-open toward the max endpoint
  else if (dir < 0) moveToUs(MIN_US);          // slow-close toward the min endpoint
  else { g_sweep = false; g_tgtUs = (int)roundf(g_curUs); }  // stop where we are
}

int angle()        { return usToAngle((int)roundf(g_curUs)); }
int microseconds() { return (int)roundf(g_curUs); }
int minUs()        { return MIN_US; }
int maxUs()        { return MAX_US; }

uint16_t speed() { return g_speed; }

void setSpeed(uint16_t dps) {
  g_speed = (dps > 1000) ? 1000 : dps;
  AppConfig::setServoSpeedDps(g_speed);
  if (g_speed == 0) LOGI("servo", "speed set to Max (instant)");
  else              LOGI("servo", "speed set to %u deg/s", g_speed);
}

void startSweep() {
  ensureAttached();
  g_sweep = true;
  g_tgtUs = (g_curUs < (MIN_US + MAX_US) / 2) ? MAX_US : MIN_US;
  LOGI("servo", "sweep started");
}

void stopSweep() {
  g_sweep = false;
  g_tgtUs = (int)roundf(g_curUs);      // stop where we are, not at the far end
  LOGI("servo", "sweep stopped");
}
bool sweeping()  { return g_sweep; }

void loop() {
  uint32_t now = millis();
  if (now - g_lastTick < MOVE_TICK_MS) return;
  float dt = (now - g_lastTick) / 1000.0f;
  g_lastTick = now;
  if (!g_servo.attached()) return;
  if (g_sweep && fabsf(g_tgtUs - g_curUs) < 1.0f)
    g_tgtUs = (g_tgtUs <= MIN_US) ? MAX_US : MIN_US;   // bounce off the ends
  if (fabsf(g_tgtUs - g_curUs) < 0.5f) return;         // settled — nothing to do
  // Slew toward the target in µs. deg/s → µs/s via US_PER_DEG. Sweeping at "Max"
  // uses a sane fixed rate instead of teleporting.
  float dps  = g_speed ? g_speed : (g_sweep ? SWEEP_DPS_UNCAP : 0);
  float step = dps ? dps * US_PER_DEG * dt : fabsf(g_tgtUs - g_curUs);
  if (fabsf(g_tgtUs - g_curUs) <= step) g_curUs = g_tgtUs;
  else g_curUs += (g_tgtUs > g_curUs) ? step : -step;
  g_servo.writeMicroseconds((int)roundf(g_curUs));
}

String statusJson() {
  int curUs = (int)roundf(g_curUs);
  bool moving = g_servo.attached() && abs(curUs - g_tgtUs) > 1;
  String j = "{";
  j += "\"pin\":"       + String(g_pin);
  j += ",\"attached\":" + String(g_servo.attached() ? "true" : "false");
  j += ",\"angle\":"    + String(usToAngle(curUs));
  j += ",\"target\":"   + String(usToAngle(g_tgtUs));
  j += ",\"us\":"       + String(curUs);
  j += ",\"targetUs\":" + String(g_tgtUs);
  j += ",\"moving\":"   + String(moving ? "true" : "false");
  j += ",\"speed\":"    + String(g_speed);
  j += ",\"sweeping\":" + String(g_sweep ? "true" : "false");
  j += ",\"min\":"      + String(MIN_US);
  j += ",\"max\":"      + String(MAX_US);
  j += "}";
  return j;
}

}  // namespace ServoController
