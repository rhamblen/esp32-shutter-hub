// ServoController — v0.1.0 single-servo bench-test driver.
//
// Phase-1 hardware bring-up: drives ONE MG90-class servo directly from an ESP32
// GPIO (default GPIO13, configurable + persisted in AppConfig) so the servo can
// be exercised before the PCA9685 / power chain exist. The "Servo test" tab in
// the web UI calls into here. Phase 2 replaces this with PCA9685 multi-servo
// control (see ADR 0002/0003) — the API here is deliberately test-only.
#pragma once
#include <Arduino.h>

namespace ServoController {
void    begin();                 // load pin from AppConfig; stays DETACHED (nothing moves at boot)
void    loop();                  // advance the non-blocking sweep; call every main loop

uint8_t pin();                   // current signal GPIO
bool    isValidPin(uint8_t g);   // true if g can drive a servo output on the ESP32-D
bool    setPin(uint8_t g);       // validate + persist + re-attach if active; false if invalid

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

String  statusJson();            // {pin,attached,angle,target,us,targetUs,moving,speed,sweeping,min,max}
}
