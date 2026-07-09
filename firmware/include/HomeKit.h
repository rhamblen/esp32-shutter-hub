// HomeKit — STUB. Phase 5: HomeSpan bridge exposing one Window Covering accessory
// per shutter (+ Siri). The System > HomeKit tab (v0.4.4) already stores the config
// (AppConfig::hkEnabled/hkBridgeName/hkSetupCode) and reads bridge state through this
// API, so wiring HomeSpan in only changes this module. Contract for that work:
//   - QR setup ID is "SHUT" (homeSpan.setQRID) — the web UI renders the pairing QR
//     with that ID baked in, so firmware and UI must stay in sync.
//   - HAP must move off port 80 (the web server owns it): homeSpan.setPort(1201).
//   - WiFi stays owned by WiFiSetup/WiFiManager; HomeSpan must not manage it.
#pragma once

namespace HomeKit {
void begin();          // no-op stub for now
bool running();        // false until HomeSpan is wired in (v0.5.0)
bool paired();         // any HomeKit controllers paired?
int  controllers();    // number of paired controllers
bool resetPairings();  // wipe pairing data; false while the bridge isn't built
}
