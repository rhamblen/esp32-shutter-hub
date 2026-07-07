// AppConfig — persisted device settings (NVS via Preferences).
//
// The single home for configuration that must survive reboots AND OTA updates
// (NVS is a separate flash partition from the app). WiFi credentials are NOT here
// — those are owned by WiFiManager in its own NVS namespace. This is where future
// phases (shutters, HomeKit, MQTT) store their settings.
#pragma once
#include <Arduino.h>

namespace AppConfig {
void     begin();                              // open NVS, load settings, bump boot count
String   deviceName();                         // used for hostname / mDNS (default "shutter-hub")
void     setDeviceName(const String &name);
uint32_t bootCount();                          // total boots (diagnostics)

// Phase-1 servo bench test — signal GPIO for the single test servo (default 13).
uint8_t  servoPin();
void     setServoPin(uint8_t gpio);

// Last OTA flash record (what + when + result), persisted across reboots/OTA.
String   lastFlashType();                       // "firmware" | "filesystem" | "none"
bool     lastFlashOk();
uint32_t lastFlashEpoch();                      // unix time, 0 if the clock wasn't synced
void     recordFlash(const String &type, bool ok, uint32_t epoch);
}
