// WiFiSetup — on-device WiFi provisioning (WiFiManager AP + captive portal).
//
// First boot (or after forgetAndReboot) raises the "Shutter-Hub-Setup" access
// point with a captive portal; the chosen network is stored in NVS and survives
// OTA updates. Mirrors rednblkx/HomeKey-ESP32's SoftAP + captive-portal pattern.
#pragma once
#include <Arduino.h>

namespace WiFiSetup {
void connect();          // blocks until connected (opens the portal if unprovisioned)
void forgetAndReboot();  // clear saved WiFi from NVS and restart into the setup AP
void setSoftAP(bool on); // bring the management AP up (reachable at 192.168.4.1) or tear it down

// Switch to a different network in place (no reboot). Persists the new credentials
// on success; on failure reverts to the previous network. Returns true if joined.
bool connectTo(const String &ssid, const String &pass);
}
