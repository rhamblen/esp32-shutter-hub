#include "ServoController.h"
#include "AppConfig.h"
#include "Diagnostics.h"

// Multi-slot µs driver. Position is tracked in MICROSECONDS (pulse width) — the
// servo's native unit — so blind calibration can land a slat far more finely than
// whole degrees. The Servo-test page still talks in degrees; those are derived
// from µs on the way in/out.
//
// v0.4.0 (ADR 0010): each physical output ("slot") carries its OWN slew state, so
// several shutters can move simultaneously — an HA "close all" slews every
// commanded channel at once. The PCA9685 generates all 16 waveforms autonomously
// in hardware; the loop() below just nudges each moving channel's pulse width
// once per 50 Hz frame (a few short I2C writes — trivial bus load). The bench
// API (writeUs, jog, run, sweep, attach…) acts on the ACTIVE slot only.
//
// The hardware layer is selected at build time (see platformio.ini / ADR 0008):
//   USE_PCA9685=0  ESP32Servo on a signal GPIO (LEDC-backed) — one slot.
//   USE_PCA9685=1  Adafruit PWM driver on PCA9685 channels over I2C — 16 slots.
// All slots stay DETACHED at boot so nothing twitches until something commands a
// move — this also dodges the attach-time current surge on a bare/USB-powered board.

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
// MG90-class pulse envelope. Wider than the classic 1000–2000 µs so the endpoints
// reach the servo's real mechanical limits during calibration.
const int      MIN_US          = 500;
const int      MAX_US          = 2500;
const uint32_t MOVE_TICK_MS    = 20;   // one 50 Hz servo frame
const uint16_t SWEEP_DPS_UNCAP = 133;  // sweep rate used when speed is "Max"
const uint32_t POS_PERSIST_MS  = 3000; // idle hold-off before writing a settled position (flash wear)

// Assembly "home": where an un-driven slot is assumed to rest after a factory-fresh boot,
// so the very first move slews from a real reference instead of snapping. Fit the servo horn
// / linkage with the arm parked HERE (slat closed) during the build. See ADR 0009.
const int HOME_US = MIN_US;

// One degree spans this many µs across the 0–180° / MIN..MAX map — used to turn
// the deg/s speed limit into a µs/s slew rate.
const float US_PER_DEG = (float)(MAX_US - MIN_US) / 180.0f;

#if USE_PCA9685
const int NSLOTS = 16;
#else
const int NSLOTS = 1;
#endif

// Per-slot slew state — one entry per physical output.
struct SlotState {
  float    cur       = (float)HOME_US;  // pulse width on the output right now
  int      tgt       = HOME_US;         // pulse width we're slewing toward
  bool     live      = false;           // attached / emitting pulses?
  bool     moving    = false;           // was mid-slew last tick (settle-edge detect)
  bool     dirty     = false;           // settled position awaiting NVS write
  uint32_t settledMs = 0;               // when it settled (debounce persistence)
};
SlotState g_s[NSLOTS];

uint16_t g_speed    = 25;              // max slew rate in deg/s; 0 = unlimited (snap)
bool     g_sweep    = false;           // bench sweep — active slot only
uint32_t g_lastTick = 0;

int   angleToUs(int a) { return map(constrain(a, 0, 180), 0, 180, MIN_US, MAX_US); }
int   usToAngle(int u) { return map(constrain(u, MIN_US, MAX_US), MIN_US, MAX_US, 0, 180); }
int   clampUs(int u)   { return constrain(u, MIN_US, MAX_US); }
int   clampSlot(int s) { return constrain(s, 0, NSLOTS - 1); }

// A GPIO that can act as a servo signal / I2C line on this board. Chip-aware at compile time so the
// ESP32-C3 builds can't offer a pin that doesn't exist — or, worse, a SPI-flash pin that HANGS/BRICKS
// the board when driven. Reused to validate the PCA9685 I2C SDA/SCL pins (I2C needs bidi-capable pins).
bool validGpio(uint8_t g) {
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  // ESP32-C3 has GPIO0–21. Exclude 11 (VDD_SPI) and 12–17 (SPI flash) — driving those bricks boot.
  // 18/19 (USB) and 20/21 (UART0) are allowed but will disturb USB-JTAG / the serial console.
  static const uint8_t ok[] = {0,1,2,3,4,5,6,7,8,9,10,18,19,20,21};
#else
  // ESP32-D (WROOM): excludes input-only 34–39, flash 6–11 and non-existent pins.
  static const uint8_t ok[] = {0,1,2,3,4,5,12,13,14,15,16,17,18,19,21,22,23,25,26,27,32,33};
#endif
  for (uint8_t p : ok) if (p == g) return true;
  return false;
}

// ---- Backend: hardware primitives (all slot-addressed) ----------------------
#if USE_PCA9685
Adafruit_PWMServoDriver g_pwm(0x40);   // default I2C address (ADR 0003)
uint8_t g_ch  = 0;                     // ACTIVE test slot (Servo-test / calibration focus)
uint8_t g_sda = 21, g_scl = 22;        // I2C bus pins

void hwInit() {
  Wire.begin(g_sda, g_scl);
  g_pwm.begin();
  g_pwm.setOscillatorFrequency(27000000);  // datasheet internal osc — trims writeMicroseconds accuracy
  g_pwm.setPWMFreq(50);                     // standard analog-servo frame rate
}
void hwWrite(int slot, int us) { g_pwm.writeMicroseconds(slot, us); }
void hwAttach(int slot)        { g_s[slot].live = true; }   // channel goes live on the next write
void hwDetach(int slot) {
  g_pwm.setPWM(slot, 0, 4096);   // full-OFF — releases the servo (no holding torque)
  g_s[slot].live = false;
}
int  activeSlot() { return g_ch; }
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
void hwWrite(int, int us) { g_servo.writeMicroseconds(us); }
void hwAttach(int) {
  if (!g_servo.attached()) {
    g_servo.setPeriodHertz(50);            // standard analog-servo frame rate
    g_servo.attach(g_pin, MIN_US, MAX_US);
  }
  g_s[0].live = true;
}
void hwDetach(int) {
  if (g_servo.attached()) g_servo.detach();
  g_s[0].live = false;
}
int  activeSlot() { return 0; }
#endif

// Record where a slot just settled so a warm reboot restores it (and the first move
// slews from that reference instead of a guess). RAM first; the NVS write is debounced
// in loop() to spare the flash during calibration jogging.
void rememberPos(int slot) { g_s[slot].dirty = true; g_s[slot].settledMs = millis(); }
void persistPos(int slot) {
  AppConfig::setServoPos(slot, (int)roundf(g_s[slot].cur));
  g_s[slot].dirty = false;
}

// Point a slot at an absolute pulse width, honouring the speed limit.
void moveToUs(int slot, int us) {
  slot = clampSlot(slot);
  if (slot == activeSlot()) g_sweep = false;   // a direct move cancels a bench sweep
  SlotState &S = g_s[slot];
  S.tgt = clampUs(us);
  hwAttach(slot);
  if (g_speed == 0) {                          // Max speed — snap straight there
    S.cur = (float)S.tgt;
    hwWrite(slot, S.tgt);
    rememberPos(slot);                         // slewed moves are remembered on settle in loop()
  }                                            // otherwise loop() slews there at g_speed
}
}  // namespace

namespace ServoController {

void begin() {
  g_speed = AppConfig::servoSpeedDps();
#if USE_PCA9685
  g_sda = AppConfig::i2cSda();
  g_scl = AppConfig::i2cScl();
  g_ch  = AppConfig::servoChannel();
#else
  g_pin = AppConfig::servoPin();
#endif
  // Restore each slot's last-known position (or assembly HOME if never driven) so
  // the first move after a warm reboot / OTA slews from where the servo actually is.
  for (int i = 0; i < NSLOTS; i++) {
    int p = AppConfig::servoPos(i);
    g_s[i].cur = (p < 0) ? (float)HOME_US : (float)p;
    g_s[i].tgt = (int)roundf(g_s[i].cur);
  }
  hwInit();
#if USE_PCA9685
  LOGI("servo", "driver ready — PCA9685 @0x40, SDA GPIO%u/SCL GPIO%u, 16 slots, test channel %u @ %d µs, all detached (idle)",
       g_sda, g_scl, g_ch, (int)roundf(g_s[g_ch].cur));
#else
  LOGI("servo", "driver ready — direct GPIO%u @ %d µs, detached (idle)", g_pin, (int)roundf(g_s[0].cur));
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
  // Just move the test focus — never drive or release a channel on selection. With
  // per-slot state (ADR 0010) an in-flight move on the old channel keeps slewing to
  // completion; each channel keeps holding its own position.
  g_sweep = false;                           // the bench sweep doesn't follow a focus change
  g_ch = ch;
  AppConfig::setServoChannel(ch);
  LOGD("servo", "test focus → channel %u (at %d µs%s)", g_ch, (int)roundf(g_s[g_ch].cur),
       g_s[g_ch].moving ? ", mid-move" : "");
  return true;
}

uint8_t sdaPin() { return g_sda; }
uint8_t sclPin() { return g_scl; }

bool setI2cPins(uint8_t sda, uint8_t scl) {
  if (!validGpio(sda) || !validGpio(scl) || sda == scl) {
    LOGW("servo", "rejected I2C pins SDA%u/SCL%u", sda, scl); return false;
  }
  g_sweep = false;
  Wire.end();
  g_sda = sda; g_scl = scl;
  AppConfig::setI2cPins(sda, scl);
  hwInit();                                 // re-open the bus on the new pins
  for (int i = 0; i < NSLOTS; i++)          // re-assert every live output on the re-opened bus
    if (g_s[i].live) hwWrite(i, (int)roundf(g_s[i].cur));
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
  if (wasAttached) { hwAttach(0); g_servo.writeMicroseconds((int)roundf(g_s[0].cur)); }
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
  int s = activeSlot();
  hwAttach(s);
  g_s[s].tgt = (int)roundf(g_s[s].cur);   // hold position — no surprise move on attach
  hwWrite(s, g_s[s].tgt);
  LOGD("servo", "attached at %d µs", g_s[s].tgt);
  return g_s[s].live;
}

void detach() {
  int s = activeSlot();
  g_sweep = false;
  g_s[s].tgt = (int)roundf(g_s[s].cur);   // freeze any in-flight move where it stopped
  hwDetach(s);
  rememberPos(s); persistPos(s);          // a deliberate stop — save the frozen position now
  LOGD("servo", "detached (released) at %d µs", (int)roundf(g_s[s].cur));
}

bool attached() { return g_s[activeSlot()].live; }

void writeAngle(int deg) { moveToUs(activeSlot(), angleToUs(constrain(deg, 0, 180))); }
void writeUs(int us)     { moveToUs(activeSlot(), us); }

void jogUs(int deltaUs) {
  // Nudge relative to the current target so repeated clicks accumulate cleanly,
  // even if a slew is still settling from the last one.
  int s = activeSlot();
  moveToUs(s, g_s[s].tgt + deltaUs);
}

void run(int dir) {
  int s = activeSlot();
  if      (dir > 0) moveToUs(s, MAX_US);       // slow-open toward the max endpoint
  else if (dir < 0) moveToUs(s, MIN_US);       // slow-close toward the min endpoint
  else { g_sweep = false; g_s[s].tgt = (int)roundf(g_s[s].cur); }  // stop where we are
}

int angle()        { return usToAngle((int)roundf(g_s[activeSlot()].cur)); }
int microseconds() { return (int)roundf(g_s[activeSlot()].cur); }
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
  int s = activeSlot();
  hwAttach(s);
  g_sweep = true;
  g_s[s].tgt = (g_s[s].cur < (MIN_US + MAX_US) / 2) ? MAX_US : MIN_US;
  LOGD("servo", "sweep started");
}

void stopSweep() {
  int s = activeSlot();
  g_sweep = false;
  g_s[s].tgt = (int)roundf(g_s[s].cur);   // stop where we are, not at the far end
  LOGD("servo", "sweep stopped");
}
bool sweeping()  { return g_sweep; }

// ---- Slot API (ADR 0010) -----------------------------------------------------

void moveSlotUs(uint8_t slot, int us) { moveToUs(slot, us); }

void stopSlot(uint8_t slot) {
  int s = clampSlot(slot);
  if (s == activeSlot()) g_sweep = false;
  g_s[s].tgt = (int)roundf(g_s[s].cur);   // freeze where it is (keeps holding torque)
}

int  slotUs(uint8_t slot)       { return (int)roundf(g_s[clampSlot(slot)].cur); }
int  slotTargetUs(uint8_t slot) { return g_s[clampSlot(slot)].tgt; }

bool slotMoving(uint8_t slot) {
  const SlotState &S = g_s[clampSlot(slot)];
  return S.live && abs((int)roundf(S.cur) - S.tgt) > 1;
}

void loop() {
  uint32_t now = millis();
  if (now - g_lastTick < MOVE_TICK_MS) return;
  float dt = (now - g_lastTick) / 1000.0f;
  g_lastTick = now;
  for (int i = 0; i < NSLOTS; i++) {
    SlotState &S = g_s[i];
    // Debounced write of a settled position (runs even while detached).
    if (S.dirty && (now - S.settledMs) > POS_PERSIST_MS) persistPos(i);
    if (!S.live) continue;
    bool sweepHere = g_sweep && i == activeSlot();
    if (sweepHere && fabsf(S.tgt - S.cur) < 1.0f)
      S.tgt = (S.tgt <= MIN_US) ? MAX_US : MIN_US;     // bounce off the ends
    if (fabsf(S.tgt - S.cur) < 0.5f) {                 // settled — nothing to do
      if (S.moving) { S.moving = false; rememberPos(i); }  // just arrived — remember where
      continue;
    }
    S.moving = true;
    // Slew toward the target in µs. deg/s → µs/s via US_PER_DEG. Sweeping at "Max"
    // uses a sane fixed rate instead of teleporting.
    float dps  = g_speed ? g_speed : (sweepHere ? SWEEP_DPS_UNCAP : 0);
    float step = dps ? dps * US_PER_DEG * dt : fabsf(S.tgt - S.cur);
    if (fabsf(S.tgt - S.cur) <= step) S.cur = S.tgt;
    else S.cur += (S.tgt > S.cur) ? step : -step;
    hwWrite(i, (int)roundf(S.cur));
  }
}

String statusJson() {
  const SlotState &S = g_s[activeSlot()];
  int curUs = (int)roundf(S.cur);
  bool moving = S.live && abs(curUs - S.tgt) > 1;
  String j = "{";
  j += "\"backend\":\""  + String(backend()) + "\"";
  j += ",\"usesPca\":"   + String(usesPca() ? "true" : "false");
  j += ",\"pin\":"       + String(pin());
  j += ",\"channel\":"   + String(channel());
  j += ",\"sda\":"       + String(sdaPin());
  j += ",\"scl\":"       + String(sclPin());
  j += ",\"attached\":"  + String(S.live ? "true" : "false");
  j += ",\"angle\":"     + String(usToAngle(curUs));
  j += ",\"target\":"    + String(usToAngle(S.tgt));
  j += ",\"us\":"        + String(curUs);
  j += ",\"targetUs\":"  + String(S.tgt);
  j += ",\"moving\":"    + String(moving ? "true" : "false");
  j += ",\"speed\":"     + String(g_speed);
  j += ",\"sweeping\":"  + String(g_sweep ? "true" : "false");
  j += ",\"min\":"       + String(MIN_US);
  j += ",\"max\":"       + String(MAX_US);
  j += "}";
  return j;
}

}  // namespace ServoController
