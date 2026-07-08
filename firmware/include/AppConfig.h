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

// Phase-1 servo bench test — signal GPIO for the single test servo (default 13),
// and the max slew rate in deg/s (default 25; 0 = unlimited/instant).
uint8_t  servoPin();
void     setServoPin(uint8_t gpio);
uint16_t servoSpeedDps();
void     setServoSpeedDps(uint16_t dps);

// Last OTA flash record (what + when + result), persisted across reboots/OTA.
String   lastFlashType();                       // "firmware" | "filesystem" | "none"
bool     lastFlashOk();
uint32_t lastFlashEpoch();                      // unix time, 0 if the clock wasn't synced
void     recordFlash(const String &type, bool ok, uint32_t epoch);

// ---- MQTT / Home Assistant (Phase 4 config, usable from v0.2.0) ----
// Broker connection + a base topic. clientId defaults to "shutter-hub-XXXX" (MAC
// suffix) when left blank. haDiscovery publishes Home Assistant MQTT discovery.
bool     mqttEnabled();
String   mqttHost();
uint16_t mqttPort();                            // default 1883
String   mqttClientId();                        // resolved (never blank)
String   mqttUser();
String   mqttPass();
String   mqttBaseTopic();                       // default "shutter-hub"; LWT = <base>/status
bool     mqttHaDiscovery();
void     setMqtt(bool enabled, const String &host, uint16_t port, const String &clientId,
                 const String &user, const String &pass, const String &baseTopic, bool haDiscovery);

// ---- Web interface authentication (Security tab) ----
bool     authEnabled();
String   authUser();
String   authPass();
void     setAuth(bool enabled, const String &user, const String &pass);

// Clear all app settings (device name, servo pin, MQTT, auth) back to defaults.
// Does NOT touch WiFi credentials (WiFiManager's own NVS namespace). Caller reboots.
void     factoryReset();
}
