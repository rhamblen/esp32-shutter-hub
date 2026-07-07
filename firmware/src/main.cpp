// ESP32 Smart Shutter Hub — firmware framework (v0.0.x)
// -----------------------------------------------------------------------------
// Thin entry point. All behaviour lives in modules (see the brief / ADR 0004):
//   AppConfig       persisted settings (NVS)                     [real]
//   Diagnostics     logging, /info, uptime, reboot               [real]
//   WiFiSetup       WiFiManager AP + captive portal (creds->NVS) [real]
//   WebUI           tabbed status page + routes + mDNS           [real]
//   Ota             ElegantOTA /update                           [real]
//   ServoController PCA9685 + MG90D                              [stub, Phase 1/2]
//   Mqtt            Home Assistant discovery                     [stub, Phase 4]
//   HomeKit         HomeSpan bridge                              [stub, Phase 5]
//   LightSensor     VEML7700 solar protection                   [stub, Phase 6]
//
// Runs on a bare ESP32 dev board — no PCA9685, servos or power hardware needed yet.
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <time.h>
#include "AppConfig.h"
#include "Diagnostics.h"
#include "WiFiSetup.h"
#include "WebUI.h"
#include "ServoController.h"
#include "Mqtt.h"
#include "HomeKit.h"
#include "LightSensor.h"

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  LOGI("main", "=== ESP32 Shutter Hub — firmware v%s ===", FW_VERSION);

  AppConfig::begin();      // load settings + bump boot count (needed before others)
  Diagnostics::begin();    // log boot reason / count
  WiFiSetup::connect();    // blocks until on WiFi (or opens the setup portal)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC clock for last-flash timestamps
  WebUI::begin();          // mDNS + web UI + OTA
  WiFiSetup::setSoftAP(AppConfig::apEnabled());  // apply persisted AP state (off by default)

  // Future subsystems — stubs today, filled in by their phases:
  ServoController::begin();
  Mqtt::begin();
  HomeKit::begin();
  LightSensor::begin();

  LOGI("main", "ready");
}

void loop() {
  WebUI::loop();
  delay(5);
}
