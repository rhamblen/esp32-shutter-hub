// ESP32 Smart Shutter Hub — firmware framework (v0.0.x)
// -----------------------------------------------------------------------------
// Thin entry point. All behaviour lives in modules (see the brief / ADR 0004):
//   AppConfig       persisted settings (NVS)                     [real]
//   Diagnostics     logging, /info, uptime, reboot               [real]
//   WiFiSetup       WiFiManager AP + captive portal (creds->NVS) [real]
//   WebUI           tabbed status page + routes + mDNS           [real]
//   Ota             custom firmware + LittleFS update             [real]
//   ServoController multi-slot µs driver, backend = GPIO | PCA9685    [real]
//   Shutters        per-blind definitions + calibration (NVS)     [Phase 2 real]
//   Mqtt            HA covers/buttons + discovery + state         [Phase 4 real, v0.4.0]
//   HomeKit         HomeSpan bridge (Window Covering / shutter)   [Phase 5 real, v0.5.0]
//   LightSensor     VEML7700 ambient light on Wire1              [Phase 6 real, v0.6.0]
//   SolarLogic      trip/clear heat-protection state machine     [Phase 6 real, v0.6.0]
//
// The `-direct` variants run on a bare ESP32 dev board; the `-pca9685` variants
// expect a PCA9685 on I2C (see platformio.ini / ADR 0008). No servos required to boot.
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <time.h>
#include <LittleFS.h>
#include "AppConfig.h"
#include "Diagnostics.h"
#include "WiFiSetup.h"
#include "WebUI.h"
#include "ServoController.h"
#include "Shutters.h"
#include "Mqtt.h"
#include "HomeKit.h"
#include "LightSensor.h"
#include "SolarLogic.h"

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
  if (LittleFS.begin(true)) {   // format-on-fail: a blank FS mounts empty (recovery page serves)
    LOGI("fs", "LittleFS mounted");
  } else {
    LOGW("fs", "LittleFS mount failed — web UI falls back to the recovery page");
  }
  WiFiSetup::connect();    // blocks until on WiFi (or opens the setup portal)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // UTC clock for last-flash timestamps
  WebUI::begin();          // mDNS + web UI + OTA

  // Future subsystems — stubs today, filled in by their phases:
  ServoController::begin();
  Shutters::begin();       // load per-blind definitions + calibration from NVS
  Mqtt::begin();
  HomeKit::begin();
  LightSensor::begin();    // VEML7700 on its own I2C bus (Wire1)
  SolarLogic::begin();     // trip/clear heat-protection state machine

  LOGI("main", "ready");
}

void loop() {
  WebUI::loop();
  ServoController::loop();   // advances the non-blocking servo sweep, if running
  Mqtt::loop();             // pump MQTT client + non-blocking reconnect
  HomeKit::loop();          // pump HomeSpan (HAP) + deferred pairing reset
  LightSensor::loop();      // sample the VEML7700 (~1 Hz)
  SolarLogic::loop();       // advance the trip/clear state machine
  delay(5);
}
