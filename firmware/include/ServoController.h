// ServoController — single-servo bench-test driver (compile-time backend, v0.3.0).
//
// Drives ONE MG90-class servo so it can be exercised from the "Servo test" web tab.
// The backend is chosen at build time (see platformio.ini / ADR 0008):
//   USE_PCA9685=0  direct GPIO  — ESP32Servo (LEDC), servo on a signal GPIO.
//   USE_PCA9685=1  PCA9685      — Adafruit PWM driver over I2C; the servo lives on a
//                                 channel (0–15), the bus on the configured SDA/SCL.
// The µs-based position core (slew, speed, sweep) is identical across backends; only
// the hardware primitives (init, emit-µs, attach/detach, addressing) differ. Still
// one active servo at a time — parallel 4-channel drive is a later phase (0002/0003).
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
}
