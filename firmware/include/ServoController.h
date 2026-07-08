// ServoController — multi-slot µs servo driver (compile-time backend, v0.4.0).
//
// The backend is chosen at build time (see platformio.ini / ADR 0008):
//   USE_PCA9685=0  direct GPIO  — ESP32Servo (LEDC), one servo on a signal GPIO.
//   USE_PCA9685=1  PCA9685      — Adafruit PWM driver over I2C; servos live on
//                                 channels (0–15), the bus on the configured SDA/SCL.
// The µs-based position core (slew, speed, sweep) is identical across backends; only
// the hardware primitives (init, emit-µs, attach/detach, addressing) differ.
//
// v0.4.0 (ADR 0010): every physical output ("slot") carries its own slew state, so
// several shutters can move at once — an HA "close all" slews all channels
// simultaneously. A slot is a PCA9685 channel on that build, or the one GPIO servo
// on a direct build. The bench/test API below acts on the ACTIVE slot (the test
// channel picked on the Servo-test page); the slot API at the bottom drives any
// output regardless of focus (used by the MQTT cover handlers).
#pragma once
#include <Arduino.h>

namespace ServoController {
void    begin();                 // load config from AppConfig; stays DETACHED (nothing moves at boot)
void    loop();                  // advance the non-blocking sweep; call every main loop

bool        usesPca();           // true on a PCA9685 build (servo addressed by channel, not GPIO)
const char *backend();           // "gpio" | "pca9685" — for the info screen + adaptive UI

// ---- Direct-GPIO addressing (USE_PCA9685=0) --------------------------------
uint8_t pin();                   // current signal GPIO
bool    isValidPin(uint8_t g);   // true if g can drive a servo output on the ESP32-D
bool    setPin(uint8_t g);       // validate + persist + re-attach if active; false if invalid

// ---- PCA9685 addressing (USE_PCA9685=1; no-ops on a direct build) ----------
uint8_t channel();               // active PCA9685 test channel (0–15)
bool    setChannel(uint8_t ch);  // validate 0–15 + persist + move drive to that channel
uint8_t sdaPin();                // I2C bus pins the PCA9685 sits on
uint8_t sclPin();
bool    setI2cPins(uint8_t sda, uint8_t scl);  // validate + persist + re-init the bus

bool    attach();                // start emitting pulses at the last angle
void    detach();                // stop pulses — releases the servo (no holding torque)
bool    attached();

void    writeAngle(int deg);     // 0..180; auto-attaches; cancels any sweep; slews at speed()
int     angle();                 // angle currently on the output (mid-move it trails the target)
int     microseconds();          // pulse width currently on the output (mid-move it trails target)

// ---- Microsecond control (Phase-2 blind calibration) -----------------------
// The Shutters page calibrates in the servo's native unit (pulse width) for finer
// control than whole degrees. These share the one physical servo and clamp to the
// full min..max envelope so calibration can hunt past the saved endpoints.
void    writeUs(int us);         // move to an absolute pulse width; auto-attaches; slews at speed()
void    jogUs(int deltaUs);      // frame-step: nudge the target by ±deltaUs, clamped to min..max
void    run(int dir);            // slow-run: +1 toward max (open), -1 toward min (close), 0 = stop
int     minUs();                 // servo pulse envelope, min
int     maxUs();                 // servo pulse envelope, max

uint16_t speed();                // max slew rate, deg/s; 0 = unlimited ("Max", snap)
void     setSpeed(uint16_t dps); // clamp to 0..1000 + persist (AppConfig)

void    startSweep();            // continuous 0<->180 sweep (non-blocking, speed-limited)
void    stopSweep();
bool    sweeping();

String  statusJson();            // {backend,pin,channel,sda,scl,attached,angle,target,us,targetUs,moving,speed,sweeping,min,max}

// ---- Slot API — drive any output, independent of the test focus (ADR 0010) --
// Slot = PCA9685 channel (0–15) on that build; the direct build has a single slot
// (the argument is clamped to 0). Every slot slews independently and concurrently
// at the shared speed(). Used by the MQTT cover handlers (Phase 4).
void    moveSlotUs(uint8_t slot, int us);   // absolute move; auto-attaches that slot
void    stopSlot(uint8_t slot);             // freeze that slot where it is (keeps holding)
int     slotUs(uint8_t slot);               // pulse width currently on that output
int     slotTargetUs(uint8_t slot);         // pulse width it is slewing toward
bool    slotMoving(uint8_t slot);           // true while that slot is mid-slew
}
