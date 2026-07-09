// HomeKit — HomeSpan bridge exposing one Window Covering per shutter (Phase 5, v0.5.0).
//
// Native HAP (no hub/bridge hardware needed). Config lives in AppConfig
// (hkEnabled/hkBridgeName/hkSetupCode); the System > HomeKit web tab reads bridge
// state through the getters below and renders the pairing QR. Coexistence contract
// with the rest of the firmware (all handled in HomeKit.cpp):
//   - HAP runs on TCP port 1201 (homeSpan.setPortNum) — the async web server keeps 80.
//   - mDNS hostname is pinned to the device name (setHostNameSuffix "") so <name>.local
//     still resolves the web UI; the http service is re-asserted after HomeSpan starts.
//   - WiFi stays owned by WiFiManager: we're already connected before HomeSpan polls, so its
//     checkConnect() sees WL_CONNECTED and never calls WiFi.begin() itself (no startup blip).
//     It's given the live creds only so it can reconnect on its own if the link later drops.
//   - QR setup ID is "SHUT" — must match the web UI's X-HM:// payload.
//   - HomeSpan runs on its OWN task (homeSpan.autoPoll in begin()) — HAP/pairing crypto would
//     otherwise monopolise the shared Arduino loop() and stall servo slewing + MQTT (it did:
//     servos/HA froze whenever the bridge was enabled). Do NOT call homeSpan.poll() from loop().
// Enabling/disabling or changing shutters takes effect on the next reboot (the accessory
// tree is built once at begin(), like a normal HomeSpan sketch).
#pragma once

namespace HomeKit {
void begin();          // start the bridge if AppConfig::hkEnabled(); no-op otherwise
void loop();           // services only the deferred pairing reset (poll runs on its own task)
bool running();        // true once the bridge is up (enabled + begun)
bool paired();         // any HomeKit controller currently paired?
int  controllers();    // number of paired controllers
bool resetPairings();  // erase HomeKit pairing data + reboot; false if the bridge isn't running
}
