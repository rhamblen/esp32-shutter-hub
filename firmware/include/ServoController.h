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

void    writeAngle(int deg);     // 0..180; auto-attaches; cancels any sweep
int     angle();                 // last commanded angle
int     microseconds();          // pulse width for the last commanded angle

void    startSweep();            // continuous 0<->180 sweep (non-blocking, stepped)
void    stopSweep();
bool    sweeping();

String  statusJson();            // {pin,attached,angle,us,sweeping,min,max}
}
