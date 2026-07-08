#include "ServoController.h"
#include "AppConfig.h"
#include "Diagnostics.h"

// Single-servo driver. Position is tracked in MICROSECONDS (pulse width) — the
// servo's native unit — so Phase-2 blind calibration can land a slat far more
// finely than whole degrees. The Servo-test page still talks in degrees; those
// are derived from µs on the way in/out.
//
// The µs core (slew/speed/sweep) below is backend-independent. The hardware layer
// is selected at build time (see platformio.ini / ADR 0008):
//   USE_PCA9685=0  ESP32Servo on a signal GPIO (LEDC-backed).
//   USE_PCA9685=1  Adafruit PWM driver on a PCA9685 channel, over I2C.
// Stays DETACHED at boot so nothing twitches until the user acts — this also dodges
// the attach-time current surge on a bare/USB-powered board.

#ifndef USE_PCA9685
#define USE_PCA9685 0
#endif

#if USE_PCA9685
  #include <Wire.h>
  #include <Adafruit_PWMServoDriver.h>
#else
  #include <ESP32Servo.h>
#endif

namespace {
// ---- Shared position state --------------------------------------------------
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

// A GPIO that can act as a servo signal / I2C line on the ESP32-D: excludes
// input-only 34–39, flash 6–11 and non-existent pins. Reused to validate the
// I2C SDA/SCL pins on the PCA9685 build (I2C needs bidirectional-capable pins).
bool validGpio(uint8_t g) {
  static const uint8_t ok[] = {0,1,2,3,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33};
  for (uint8_t p : ok) if (p == g) return true;
  return false;
}

// ---- Backend: hardware primitives ------------------------------------------
#if USE_PCA9685
Adafruit_PWMServoDriver g_pwm(0x40);   // default I2C address (ADR 0003)
uint8_t g_ch       = 0;                // active test channel 0–15
uint8_t g_sda      = 21, g_scl = 22;   // I2C bus pins
bool    g_attached = false;            // PCA9685 has no attach() — track it ourselves
// Last commanded pulse width per channel. Selecting a channel restores its own
// position (never the previous channel's), so switching focus moves nothing — a
// safety must when each channel is a different blind. Default mid-travel until first driven.
float   g_chPos[16];

void hwInit() {
  Wire.begin(g_sda, g_scl);
  g_pwm.begin();
  g_pwm.setOscillatorFrequency(27000000);  // datasheet internal osc — trims writeMicroseconds accuracy
  g_pwm.setPWMFreq(50);                     // standard analog-servo frame rate
}
bool hwAttached()      { return g_attached; }
void hwWriteUs(int us) { g_pwm.writeMicroseconds(g_ch, us); }
void hwEnsureAttached(){ g_attached = true; }  // channel goes live on the next write
void hwDetach() {
  g_pwm.setPWM(g_ch, 0, 4096);   // full-OFF — releases the servo (no holding torque)
  g_attached = false;
}
#else
Servo   g_servo;
uint8_t g_pin = 13;               // signal GPIO

void hwInit() {
  // ESP32Servo shares the four LEDC hardware timers; hand them over up front.
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
}
bool hwAttached()      { return g_servo.attached(); }
void hwWriteUs(int us) { g_servo.writeMicroseconds(us); }
void hwEnsureAttached(){
  if (!g_servo.attached()) {
    g_servo.setPeriodHertz(50);            // standard analog-servo frame rate
    g_servo.attach(g_pin, MIN_US, MAX_US);
  }
}
void hwDetach() { if (g_servo.attached()) g_servo.detach(); }
#endif

// Point the servo at an absolute pulse width, honouring the speed limit.
void moveToUs(int us) {
  g_sweep = false;                         // a direct move cancels an in-progress sweep
  g_tgtUs = clampUs(us);
  hwEnsureAttached();
  if (g_speed == 0) {                      // Max speed — snap straight there
    g_curUs = g_tgtUs;
    hwWriteUs(g_tgtUs);
  }                                        // otherwise loop() slews there at g_speed
}
}  // namespace

namespace ServoController {

void begin() {
  g_speed = AppConfig::servoSpeedDps();
#if USE_PCA9685
  g_sda = AppConfig::i2cSda();
  g_scl = AppConfig::i2cScl();
  g_ch  = AppConfig::servoChannel();
  for (float &p : g_chPos) p = g_curUs;   // assume mid-travel per channel until first driven
  hwInit();
  LOGI("servo", "driver ready — PCA9685 @0x40, SDA GPIO%u/SCL GPIO%u, channel %u, detached (idle)",
       g_sda, g_scl, g_ch);
#else
  g_pin = AppConfig::servoPin();
  hwInit();
  LOGI("servo", "driver ready — direct GPIO%u, detached (idle)", g_pin);
#endif
}

bool        usesPca() { return USE_PCA9685; }
const char *backend() { return USE_PCA9685 ? "pca9685" : "gpio"; }

bool isValidPin(uint8_t g) { return validGpio(g); }

#if USE_PCA9685
uint8_t pin()     { return g_ch; }          // no signal GPIO on this build; report the channel
bool    setPin(uint8_t) { return false; }   // use setChannel() on a PCA9685 build

uint8_t channel() { return g_ch; }

bool setChannel(uint8_t ch) {
  if (ch > 15) { LOGW("servo", "rejected channel %u (0–15 only)", ch); return false; }
  if (ch == g_ch) return true;
  // Just move the control focus — never drive or release a channel on selection.
  // Each channel keeps holding its own last value; the next move slews from there.
  g_chPos[g_ch] = g_curUs;                   // remember where we left the current channel
  g_sweep = false;
  g_ch = ch;
  AppConfig::setServoChannel(ch);
  g_curUs = g_chPos[ch];                     // restore this channel's last commanded position
  g_tgtUs = (int)roundf(g_curUs);            // settled → loop() makes no move
  LOGI("servo", "test channel set to %u (holding %d µs)", g_ch, g_tgtUs);
  return true;
}

uint8_t sdaPin() { return g_sda; }
uint8_t sclPin() { return g_scl; }

bool setI2cPins(uint8_t sda, uint8_t scl) {
  if (!validGpio(sda) || !validGpio(scl) || sda == scl) {
    LOGW("servo", "rejected I2C pins SDA%u/SCL%u", sda, scl); return false;
  }
  bool wasAttached = g_attached;
  g_sweep = false;
  if (wasAttached) hwDetach();
  Wire.end();
  g_sda = sda; g_scl = scl;
  AppConfig::setI2cPins(sda, scl);
  hwInit();                                 // re-open the bus on the new pins
  if (wasAttached) { hwEnsureAttached(); hwWriteUs((int)roundf(g_curUs)); }
  LOGI("servo", "I2C pins set to SDA GPIO%u / SCL GPIO%u", g_sda, g_scl);
  return true;
}
#else
uint8_t pin() { return g_pin; }

bool setPin(uint8_t g) {
  if (!isValidPin(g)) { LOGW("servo", "rejected GPIO%u (not output-capable)", g); return false; }
  bool wasAttached = g_servo.attached();
  g_sweep = false;
  if (wasAttached) g_servo.detach();
  g_pin = g;
  AppConfig::setServoPin(g);
  if (wasAttached) { hwEnsureAttached(); g_servo.writeMicroseconds((int)roundf(g_curUs)); }
  LOGI("servo", "signal pin set to GPIO%u", g_pin);
  return true;
}

// PCA9685-only accessors — harmless no-ops on a direct-GPIO build.
uint8_t channel()                  { return 0; }
bool    setChannel(uint8_t)        { return false; }
uint8_t sdaPin()                   { return 255; }
uint8_t sclPin()                   { return 255; }
bool    setI2cPins(uint8_t, uint8_t) { return false; }
#endif

bool attach() {
  hwEnsureAttached();
  g_tgtUs = (int)roundf(g_curUs);      // hold position — no surprise move on attach
  hwWriteUs(g_tgtUs);
  LOGI("servo", "attached at %d µs", g_tgtUs);
  return hwAttached();
}

void detach() {
  g_sweep = false;
  g_tgtUs = (int)roundf(g_curUs);      // freeze any in-flight move where it stopped
  hwDetach();
  LOGI("servo", "detached (released)");
}

bool attached() { return hwAttached(); }

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
  hwEnsureAttached();
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
  if (!hwAttached()) return;
  if (g_sweep && fabsf(g_tgtUs - g_curUs) < 1.0f)
    g_tgtUs = (g_tgtUs <= MIN_US) ? MAX_US : MIN_US;   // bounce off the ends
  if (fabsf(g_tgtUs - g_curUs) < 0.5f) return;         // settled — nothing to do
  // Slew toward the target in µs. deg/s → µs/s via US_PER_DEG. Sweeping at "Max"
  // uses a sane fixed rate instead of teleporting.
  float dps  = g_speed ? g_speed : (g_sweep ? SWEEP_DPS_UNCAP : 0);
  float step = dps ? dps * US_PER_DEG * dt : fabsf(g_tgtUs - g_curUs);
  if (fabsf(g_tgtUs - g_curUs) <= step) g_curUs = g_tgtUs;
  else g_curUs += (g_tgtUs > g_curUs) ? step : -step;
  hwWriteUs((int)roundf(g_curUs));
}

String statusJson() {
  int curUs = (int)roundf(g_curUs);
  bool moving = hwAttached() && abs(curUs - g_tgtUs) > 1;
  String j = "{";
  j += "\"backend\":\""  + String(backend()) + "\"";
  j += ",\"usesPca\":"   + String(usesPca() ? "true" : "false");
  j += ",\"pin\":"       + String(pin());
  j += ",\"channel\":"   + String(channel());
  j += ",\"sda\":"       + String(sdaPin());
  j += ",\"scl\":"       + String(sclPin());
  j += ",\"attached\":"  + String(hwAttached() ? "true" : "false");
  j += ",\"angle\":"     + String(usToAngle(curUs));
  j += ",\"target\":"    + String(usToAngle(g_tgtUs));
  j += ",\"us\":"        + String(curUs);
  j += ",\"targetUs\":"  + String(g_tgtUs);
  j += ",\"moving\":"    + String(moving ? "true" : "false");
  j += ",\"speed\":"     + String(g_speed);
  j += ",\"sweeping\":"  + String(g_sweep ? "true" : "false");
  j += ",\"min\":"       + String(MIN_US);
  j += ",\"max\":"       + String(MAX_US);
  j += "}";
  return j;
}

}  // namespace ServoController
