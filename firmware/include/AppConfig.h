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

// PCA9685 build variant only (USE_PCA9685=1) — the I2C bus pins the PCA9685 (and
// later the VEML7700) sit on, and which channel the Servo-test page drives. Ignored
// by the direct-GPIO build, but stored unconditionally so a variant swap keeps them.
uint8_t  i2cSda();                              // default 21 (ESP32-D SDA)
uint8_t  i2cScl();                              // default 22 (ESP32-D SCL)
void     setI2cPins(uint8_t sda, uint8_t scl);
uint8_t  servoChannel();                        // PCA9685 test channel, default 0
void     setServoChannel(uint8_t ch);

// Last-known servo position per drive slot, in µs, so a warm reboot/OTA restores where
// each servo was (the first move then slews from there instead of snapping). Direct build
// uses slot 0; PCA9685 build uses slot = channel (0–15). Returns -1 if never stored.
int      servoPos(uint8_t slot);
void     setServoPos(uint8_t slot, int us);

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

// ---- HomeKit / Apple Home (Phase 5 config; System > HomeKit tab from v0.4.4) ----
// Bridge name defaults to the device name. Setup code is 8 digits; the default is
// 748-88-377 — "SHUTTERS" on a phone keypad (HomeSpan's own default 466-37-726 is
// "HOMESPAN" by the same trick, so ours is deliberately distinct).
bool     hkEnabled();
String   hkBridgeName();                        // resolved (never blank)
String   hkSetupCode();                         // digits only, e.g. "74888377"
void     setHomeKit(bool enabled, const String &name, const String &code);

// ---- Light sensor + solar heat protection (Phase 6, v0.6.0; bus choice v0.6.1) ----
// The VEML7700 bus is a setting (ADR 0012):
//   BUS_DEDICATED  its own Wire1 on lsSda/lsScl (default 25/26) — the default and the
//                  recommendation: a sensor-lead fault can't wedge the servo bus (ADR 0011).
//                  Needs a SoC with two I2C controllers, so NOT available on the ESP32-C3.
//   BUS_SHARED     rides the PCA9685's Wire on i2cSda/i2cScl (21/22). Always available;
//                  the only option on a one-controller chip. Re-couples the two devices.
// `type` reserves an analog LDR fallback (0 = VEML7700, 1 = analog); only VEML7700 is built.
enum SolarTarget : uint8_t { TGT_OPEN = 0, TGT_CLOSED = 1, TGT_DAYLIGHT = 2, TGT_PRIVACY = 3, TGT_NONE = 4 };
enum SensorBus   : uint8_t { BUS_DEDICATED = 0, BUS_SHARED = 1 };

bool     lsEnabled();
uint8_t  lsType();                             // 0 = VEML7700, 1 = analog LDR (reserved)
uint8_t  lsBus();                              // SensorBus — the *preference*; LightSensor
                                               // clamps it to BUS_SHARED on a 1-controller SoC
uint8_t  lsSda();                              // dedicated-bus SDA, default 25 (unused when shared)
uint8_t  lsScl();                              // dedicated-bus SCL, default 26 (unused when shared)
void     setLightSensor(bool enabled, uint8_t type, uint8_t bus, uint8_t sda, uint8_t scl);

// Solar automation: trip to the bright target when lux stays above tripLux for tripSecs;
// return to the clear target when lux stays below clearLux for clearSecs. Targets are a
// SolarTarget; TGT_NONE = "do nothing" (on clear that means release the suspend but leave
// slats put). Defaults: 60000 lx / 10 min → Privacy, 30000 lx / 20 min → do nothing.
bool     solarEnabled();
uint32_t solarTripLux();
uint16_t solarTripSecs();
uint32_t solarClearLux();
uint16_t solarClearSecs();
uint8_t  solarBrightTarget();                  // SolarTarget applied on trip
uint8_t  solarClearTarget();                   // SolarTarget applied on clear
void     setSolar(bool enabled, uint32_t tripLux, uint16_t tripSecs,
                  uint32_t clearLux, uint16_t clearSecs, uint8_t brightTarget, uint8_t clearTarget);

// ---- Web interface authentication (Security tab) ----
bool     authEnabled();
String   authUser();
String   authPass();
void     setAuth(bool enabled, const String &user, const String &pass);

// Clear all app settings (device name, servo pin, MQTT, HomeKit, light/solar, auth) to defaults.
// Does NOT touch WiFi credentials (WiFiManager's own NVS namespace). Caller reboots.
void     factoryReset();
}
