// SolarLogic — solar heat-protection state machine (Phase 6, v0.6.0).
//
// Watches LightSensor::lux() against the AppConfig trip/clear thresholds with dwell
// timers (hysteresis, so passing clouds don't flap the shutters):
//
//   IDLE ──lux ≥ trip for tripSecs──▶ TRIPPED     (drive to the bright target)
//   TRIPPED ──lux ≤ clear for clearSecs──▶ IDLE   (apply the clear target)
//
// The intermediate COUNTING states hold the dwell timer; lux crossing back cancels it.
// A target of TGT_NONE ("do nothing") advances the state but drives nothing — so the
// default clear = none leaves the slats where the trip left them, and bright = none
// makes the hub a pure sensor/reporter.
//
// Manual-override: a user move on a shutter (web recall/jog, MQTT command) suspends
// automation on THAT shutter for 2 h via notifyManualMove(); suspended shutters are
// skipped when an action fires, but the hub-wide state still tracks the light.
#pragma once
#include <Arduino.h>

namespace SolarLogic {
enum State : uint8_t { IDLE = 0, COUNT_TRIP = 1, TRIPPED = 2, COUNT_CLEAR = 3 };

void        begin();
void        loop();

State       state();
const char *stateText();          // "disabled" | "idle" | "counting-trip" | "tripped" | "counting-clear"
uint32_t    secondsRemaining();   // dwell countdown in a COUNTING state, else 0

void        notifyManualMove(const String &id);   // user moved this shutter → suspend 2 h
int         suspendedCount();     // shutters currently under manual-override suspend
}
