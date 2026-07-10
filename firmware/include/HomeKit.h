// HomeKit — HomeSpan bridge exposing one Window Covering per shutter (Phase 5, v0.5.0).
//
// Native HAP (no hub/bridge hardware needed). Config lives in AppConfig
// (hkEnabled/hkBridgeName/hkSetupCode); the System > HomeKit web tab reads bridge
// state through the getters below and renders the pairing QR. Coexistence contract
// with the rest of the firmware (all handled in HomeKit.cpp):
//   - HAP runs on TCP port 1201 (homeSpan.setPortNum) — the async web server keeps 80.
//   - mDNS is initialised by WebUI::begin() on the MAIN thread (hostname + _http._tcp), for both
//     modes. HomeSpan then ADDS its _hap._tcp service to that already-running responder (its own
//     mdns_init() is a no-op). hostName is pinned to the device name (setHostNameSuffix "").
//   - HomeSpan 1.9.1's hostname self-check uses sscanf("%m…"), unsupported by newlib-nano → it hit
//     while(1) PROGRAM HALTED and the bridge never advertised/opened 1201 (the v0.7.0 bug). Fixed by
//     a vendored patch — see firmware/patches/. Don't drop that patch.
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
