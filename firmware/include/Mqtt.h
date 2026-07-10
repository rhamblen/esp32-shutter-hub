// Mqtt — MQTT client + Home Assistant integration (v0.4.0, Phase 4).
//
// Config lives in AppConfig (broker, credentials, base topic, HA discovery on/off).
// Beyond the hub plumbing (availability LWT, diagnostic sensors, hub state JSON),
// each configured shutter is one HA `cover` plus six `button` entities, all via MQTT
// discovery — see ADR 0005 for the topic contract and ADR 0010 for concurrent drive:
//   subscribe  <base>/cover/<id>/set           OPEN | CLOSE | STOP
//   subscribe  <base>/cover/<id>/position/set  0–100
//   subscribe  <base>/cover/<id>/cmd           jog_open | jog_close |
//                                              recall:daylight|privacy | save:daylight|privacy
//   publish    <base>/cover/<id>/position      0–100                       (retained)
//   publish    <base>/cover/<id>/state         opening|closing|open|closed|stopped (retained)
//
// Solar heat protection (Phase 6, v0.6.0) adds a hub-wide block — an illuminance sensor, a
// brightness-percent sensor, a state sensor, an enable switch, and two writable lux thresholds
// (the `number` entities an HA-side calibration card writes back into):
//   subscribe  <base>/solar/enable/set         ON | OFF
//   subscribe  <base>/solar/trip_lux/set       lux (must stay above clear_lux)
//   subscribe  <base>/solar/clear_lux/set      lux (must stay below trip_lux)
//   publish    <base>/solar/lux                current lux                 (retained)
//   publish    <base>/solar/brightness         0-100 %, log scale          (retained)
//   publish    <base>/solar/state              disabled|idle|counting-trip|tripped|counting-clear
//   publish    <base>/solar/{enable,trip_lux,clear_lux}   echoed config    (retained)
//
// `brightness` is a DISPLAY value (0 dark … 100 full sun, one lux decade per 20 points). The
// state machine trips on raw `lux`; the log curve compresses the 30-60 k band the thresholds
// live in, so never drive automation from it.
//
// Uses PubSubClient (blocking). Safe because the web UI is served by the async
// web server on its own task — a stalled broker never freezes the interface.
#pragma once
#include <Arduino.h>

namespace Mqtt {
void   begin();           // load config; connection is established lazily in loop()
void   loop();            // pump the client + reconnect + publish shutter state changes
void   reconfigure();     // re-read AppConfig and reconnect (call after the MQTT page applies)
void   shuttersChanged(); // flag a shutter config mutation — discovery + state re-publish
                          // happen on the next loop() (safe to call from the web task)
void   solarChanged();    // flag a solar config mutation — re-publish the solar state topics
                          // on the next loop() (safe to call from the web task)

bool   connected();
String stateText();       // "disabled" | "connecting" | "connected" | "error: ..."
}
