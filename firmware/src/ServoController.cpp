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
int      g_angle    = 90;        // last commanded angle (0..180)
bool     g_sweep    = false;
int      g_sweepDir = 1;
uint32_t g_lastStep = 0;

// MG90-class pulse envelope. Wider than the classic 1000–2000 µs so the endpoints
// reach the servo's real mechanical limits during calibration.
const int      MIN_US         = 500;
const int      MAX_US         = 2500;
const int      SWEEP_STEP_DEG = 2;
const uint32_t SWEEP_STEP_MS  = 15;

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
  g_pin = AppConfig::servoPin();
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
  if (wasAttached) { ensureAttached(); g_servo.write(g_angle); }
  LOGI("servo", "signal pin set to GPIO%u", g_pin);
  return true;
}

bool attach() {
  ensureAttached();
  g_servo.write(g_angle);
  LOGI("servo", "attached on GPIO%u at %d deg", g_pin, g_angle);
  return g_servo.attached();
}

void detach() {
  g_sweep = false;
  if (g_servo.attached()) g_servo.detach();
  LOGI("servo", "detached (released)");
}

bool attached() { return g_servo.attached(); }

void writeAngle(int deg) {
  g_sweep = false;                 // a manual move cancels an in-progress sweep
  g_angle = constrain(deg, 0, 180);
  ensureAttached();
  g_servo.write(g_angle);
}

int angle()        { return g_angle; }
int microseconds() { return angleToUs(g_angle); }

void startSweep() {
  ensureAttached();
  g_sweep    = true;
  g_lastStep = millis();
  LOGI("servo", "sweep started");
}

void stopSweep() { g_sweep = false; LOGI("servo", "sweep stopped"); }
bool sweeping()  { return g_sweep; }

void loop() {
  if (!g_sweep) return;
  uint32_t now = millis();
  if (now - g_lastStep < SWEEP_STEP_MS) return;
  g_lastStep = now;
  g_angle += g_sweepDir * SWEEP_STEP_DEG;
  if (g_angle >= 180)     { g_angle = 180; g_sweepDir = -1; }
  else if (g_angle <= 0)  { g_angle = 0;   g_sweepDir =  1; }
  g_servo.write(g_angle);
}

String statusJson() {
  String j = "{";
  j += "\"pin\":"       + String(g_pin);
  j += ",\"attached\":" + String(g_servo.attached() ? "true" : "false");
  j += ",\"angle\":"    + String(g_angle);
  j += ",\"us\":"       + String(angleToUs(g_angle));
  j += ",\"sweeping\":" + String(g_sweep ? "true" : "false");
  j += ",\"min\":"      + String(MIN_US);
  j += ",\"max\":"      + String(MAX_US);
  j += "}";
  return j;
}

}  // namespace ServoController
