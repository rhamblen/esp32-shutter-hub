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

bool   connected();
String stateText();       // "disabled" | "connecting" | "connected" | "error: ..."
}
