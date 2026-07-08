// Mqtt — MQTT client + Home Assistant discovery (v0.2.0 scaffold).
//
// Config lives in AppConfig (broker, credentials, base topic, HA discovery on/off).
// v0.2.0 brings up the connection, availability (LWT), and discovery for the hub's
// diagnostic sensors so the device appears in Home Assistant. Per-shutter cover
// entities are wired in Phase 4 once ServoController drives real positions.
//
// Uses PubSubClient (blocking). Safe because the web UI is served by the async
// web server on its own task — a stalled broker never freezes the interface.
#pragma once
#include <Arduino.h>

namespace Mqtt {
void   begin();       // load config; connection is established lazily in loop()
void   loop();        // pump the client + non-blocking reconnect; call every main loop
void   reconfigure(); // re-read AppConfig and reconnect (call after the MQTT page applies)

bool   connected();
String stateText();   // "disabled" | "connecting" | "connected" | "error: ..."
}
