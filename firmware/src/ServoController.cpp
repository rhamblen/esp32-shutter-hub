#include "ServoController.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include <ESP32Servo.h>

// Single-servo bench driver (Phase 1). Uses the ESP32Servo library (LEDC-backed).
// It stays DETACHED at boot so nothing twitches until the user acts from the UI —
// this also avoids the attach-time current surge on a bare/USB-powered board.
namespace {
Servo    g_servo;
uint8_t  g_pin      = 13;
float    g_current  = 90;        // angle actually on the output right now (0..180)
int      g_target   = 90;        // angle we're moving toward
uint16_t g_speed    = 25;        // max slew rate in deg/s; 0 = unlimited (snap)
bool     g_sweep    = false;
uint32_t g_lastTick = 0;

// MG90-class pulse envelope. Wider than the classic 1000–2000 µs so the endpoints
// reach the servo's real mechanical limits during calibration.
const int      MIN_US          = 500;
const int      MAX_US          = 2500;
const uint32_t MOVE_TICK_MS    = 20;   // one 50 Hz servo frame
const uint16_t SWEEP_DPS_UNCAP = 133;  // sweep rate used when speed is "Max"

int angleToUs(int a) { return map(a, 0, 180, MIN_US, MAX_US); }

void ensureAttached() {
  if (!g_servo.attached()) {
    g_servo.setPeriodHertz(50);            // standard analog-servo frame rate
    g_servo.attach(g_pin, MIN_US, MAX_US);
  }
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
  LOGI("servo", "bench driver ready — signal GPIO%u, detached (idle)", g_pin);
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
  if (wasAttached) { ensureAttached(); g_servo.write((int)roundf(g_current)); }
  LOGI("servo", "signal pin set to GPIO%u", g_pin);
  return true;
}

bool attach() {
  ensureAttached();
  g_target = (int)roundf(g_current);   // hold position — no surprise move on attach
  g_servo.write(g_target);
  LOGI("servo", "attached on GPIO%u at %d deg", g_pin, g_target);
  return g_servo.attached();
}

void detach() {
  g_sweep  = false;
  g_target = (int)roundf(g_current);   // freeze any in-flight move where it stopped
  if (g_servo.attached()) g_servo.detach();
  LOGI("servo", "detached (released)");
}

bool attached() { return g_servo.attached(); }

void writeAngle(int deg) {
  g_sweep  = false;                // a manual move cancels an in-progress sweep
  g_target = constrain(deg, 0, 180);
  ensureAttached();
  if (g_speed == 0) {              // Max speed — snap straight to the target
    g_current = g_target;
    g_servo.write(g_target);
  }                                // otherwise loop() slews there at g_speed
}

int angle()        { return (int)roundf(g_current); }
int microseconds() { return angleToUs((int)roundf(g_current)); }

uint16_t speed() { return g_speed; }

void setSpeed(uint16_t dps) {
  g_speed = (dps > 1000) ? 1000 : dps;
  AppConfig::setServoSpeedDps(g_speed);
  if (g_speed == 0) LOGI("servo", "speed set to Max (instant)");
  else              LOGI("servo", "speed set to %u deg/s", g_speed);
}

void startSweep() {
  ensureAttached();
  g_sweep  = true;
  g_target = (g_current < 90) ? 180 : 0;
  LOGI("servo", "sweep started");
}

void stopSweep() {
  g_sweep  = false;
  g_target = (int)roundf(g_current);   // stop where we are, not at the far end
  LOGI("servo", "sweep stopped");
}
bool sweeping()  { return g_sweep; }

void loop() {
  uint32_t now = millis();
  if (now - g_lastTick < MOVE_TICK_MS) return;
  float dt = (now - g_lastTick) / 1000.0f;
  g_lastTick = now;
  if (!g_servo.attached()) return;
  if (g_sweep && fabsf(g_target - g_current) < 0.5f)
    g_target = (g_target == 0) ? 180 : 0;          // bounce off the ends
  if (fabsf(g_target - g_current) < 0.01f) return; // settled — nothing to do
  // Slew toward the target. Sweeping at "Max" uses a sane fixed rate instead of teleporting.
  float rate = g_speed ? g_speed : (g_sweep ? SWEEP_DPS_UNCAP : 0);
  float step = rate ? rate * dt : fabsf(g_target - g_current);
  if (fabsf(g_target - g_current) <= step) g_current = g_target;
  else g_current += (g_target > g_current) ? step : -step;
  g_servo.write((int)roundf(g_current));
}

String statusJson() {
  int cur = (int)roundf(g_current);
  String j = "{";
  j += "\"pin\":"       + String(g_pin);
  j += ",\"attached\":" + String(g_servo.attached() ? "true" : "false");
  j += ",\"angle\":"    + String(cur);
  j += ",\"target\":"   + String(g_target);
  j += ",\"moving\":"   + String((g_servo.attached() && cur != g_target) ? "true" : "false");
  j += ",\"speed\":"    + String(g_speed);
  j += ",\"us\":"       + String(angleToUs(cur));
  j += ",\"sweeping\":" + String(g_sweep ? "true" : "false");
  j += ",\"min\":"      + String(MIN_US);
  j += ",\"max\":"      + String(MAX_US);
  j += "}";
  return j;
}

}  // namespace ServoController
