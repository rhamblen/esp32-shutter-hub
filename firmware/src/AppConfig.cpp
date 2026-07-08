#include "AppConfig.h"
#include <Preferences.h>
#include <WiFi.h>

namespace {
Preferences prefs;
const char *NS = "shutterhub";
String   g_deviceName = "shutter-hub";
uint32_t g_bootCount  = 0;
String   g_lfType     = "none";
bool     g_lfOk       = false;
uint32_t g_lfEpoch    = 0;
uint8_t  g_servoPin   = 13;
uint16_t g_servoSpd   = 25;       // deg/s; 0 = unlimited
uint8_t  g_i2cSda     = 21;       // PCA9685 build: I2C SDA (ESP32-D default)
uint8_t  g_i2cScl     = 22;       // PCA9685 build: I2C SCL (ESP32-D default)
uint8_t  g_servoCh    = 0;        // PCA9685 build: channel driven by the Servo-test page

// MQTT / Home Assistant
bool     g_mqEn       = false;
String   g_mqHost     = "";
uint16_t g_mqPort     = 1883;
String   g_mqClient   = "";       // blank => derive from MAC
String   g_mqUser     = "";
String   g_mqPass     = "";
String   g_mqBase     = "shutter-hub";
bool     g_mqHaDisc   = true;

// Web auth
bool     g_authEn     = false;
String   g_authUser   = "admin";
String   g_authPass   = "";
}

namespace AppConfig {

void begin() {
  prefs.begin(NS, false);                       // read/write namespace
  g_deviceName = prefs.getString("devName", "shutter-hub");
  g_lfType     = prefs.getString("lfType", "none");
  g_lfOk       = prefs.getBool("lfOk", false);
  g_lfEpoch    = prefs.getUInt("lfEpoch", 0);
  g_servoPin   = prefs.getUChar("servoPin", 13);
  g_servoSpd   = prefs.getUShort("servoSpd", 25);
  g_i2cSda     = prefs.getUChar("i2cSda", 21);
  g_i2cScl     = prefs.getUChar("i2cScl", 22);
  g_servoCh    = prefs.getUChar("servoCh", 0);

  g_mqEn       = prefs.getBool("mqEn", false);
  g_mqHost     = prefs.getString("mqHost", "");
  g_mqPort     = prefs.getUShort("mqPort", 1883);
  g_mqClient   = prefs.getString("mqClient", "");
  g_mqUser     = prefs.getString("mqUser", "");
  g_mqPass     = prefs.getString("mqPass", "");
  g_mqBase     = prefs.getString("mqBase", "shutter-hub");
  g_mqHaDisc   = prefs.getBool("mqHaDisc", true);

  g_authEn     = prefs.getBool("authEn", false);
  g_authUser   = prefs.getString("authUser", "admin");
  g_authPass   = prefs.getString("authPass", "");

  g_bootCount  = prefs.getUInt("bootCount", 0) + 1;
  prefs.putUInt("bootCount", g_bootCount);
}

String deviceName() { return g_deviceName; }

void setDeviceName(const String &name) {
  g_deviceName = name;
  prefs.putString("devName", name);
}

uint32_t bootCount() { return g_bootCount; }

uint8_t servoPin() { return g_servoPin; }

void setServoPin(uint8_t gpio) {
  g_servoPin = gpio;
  prefs.putUChar("servoPin", gpio);
}

uint16_t servoSpeedDps() { return g_servoSpd; }

void setServoSpeedDps(uint16_t dps) {
  g_servoSpd = dps;
  prefs.putUShort("servoSpd", dps);
}

uint8_t i2cSda() { return g_i2cSda; }
uint8_t i2cScl() { return g_i2cScl; }

void setI2cPins(uint8_t sda, uint8_t scl) {
  g_i2cSda = sda; g_i2cScl = scl;
  prefs.putUChar("i2cSda", sda);
  prefs.putUChar("i2cScl", scl);
}

uint8_t servoChannel() { return g_servoCh; }

void setServoChannel(uint8_t ch) {
  g_servoCh = ch;
  prefs.putUChar("servoCh", ch);
}

int servoPos(uint8_t slot) {
  if (slot > 15) return -1;
  char k[8]; snprintf(k, sizeof k, "svp%u", slot);
  return prefs.getInt(k, -1);                    // -1 => never stored (assume home)
}

void setServoPos(uint8_t slot, int us) {
  if (slot > 15) return;
  char k[8]; snprintf(k, sizeof k, "svp%u", slot);
  prefs.putInt(k, us);
}

String   lastFlashType()  { return g_lfType; }
bool     lastFlashOk()    { return g_lfOk; }
uint32_t lastFlashEpoch() { return g_lfEpoch; }

void recordFlash(const String &type, bool ok, uint32_t epoch) {
  g_lfType = type; g_lfOk = ok; g_lfEpoch = epoch;
  prefs.putString("lfType", type);
  prefs.putBool("lfOk", ok);
  prefs.putUInt("lfEpoch", epoch);
}

// ---- MQTT / Home Assistant ----

bool     mqttEnabled()     { return g_mqEn; }
String   mqttHost()        { return g_mqHost; }
uint16_t mqttPort()        { return g_mqPort; }
String   mqttUser()        { return g_mqUser; }
String   mqttPass()        { return g_mqPass; }
String   mqttBaseTopic()   { return g_mqBase.length() ? g_mqBase : String("shutter-hub"); }
bool     mqttHaDiscovery() { return g_mqHaDisc; }

String mqttClientId() {
  if (g_mqClient.length()) return g_mqClient;
  String mac = WiFi.macAddress();               // AA:BB:CC:DD:EE:FF
  mac.replace(":", "");
  return "shutter-hub-" + mac.substring(6);      // last 3 bytes -> 6 hex chars
}

void setMqtt(bool enabled, const String &host, uint16_t port, const String &clientId,
             const String &user, const String &pass, const String &baseTopic, bool haDiscovery) {
  g_mqEn = enabled; g_mqHost = host; g_mqPort = port ? port : 1883;
  g_mqClient = clientId; g_mqUser = user; g_mqPass = pass;
  g_mqBase = baseTopic.length() ? baseTopic : String("shutter-hub");
  g_mqHaDisc = haDiscovery;
  prefs.putBool("mqEn", g_mqEn);
  prefs.putString("mqHost", g_mqHost);
  prefs.putUShort("mqPort", g_mqPort);
  prefs.putString("mqClient", g_mqClient);
  prefs.putString("mqUser", g_mqUser);
  prefs.putString("mqPass", g_mqPass);
  prefs.putString("mqBase", g_mqBase);
  prefs.putBool("mqHaDisc", g_mqHaDisc);
}

// ---- Web interface authentication ----

bool   authEnabled() { return g_authEn && g_authPass.length(); }  // never lock out without a password
String authUser()    { return g_authUser.length() ? g_authUser : String("admin"); }
String authPass()    { return g_authPass; }

void setAuth(bool enabled, const String &user, const String &pass) {
  g_authEn = enabled; g_authUser = user; g_authPass = pass;
  prefs.putBool("authEn", g_authEn);
  prefs.putString("authUser", g_authUser);
  prefs.putString("authPass", g_authPass);
}

void factoryReset() {
  prefs.clear();                 // wipe the "shutterhub" namespace (keeps WiFi creds elsewhere)
  prefs.putUInt("bootCount", g_bootCount);   // preserve the boot counter across a config reset
}

}  // namespace AppConfig
