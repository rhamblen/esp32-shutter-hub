#include "AppConfig.h"
#include <Preferences.h>

namespace {
Preferences prefs;
const char *NS = "shutterhub";
String   g_deviceName = "shutter-hub";
uint32_t g_bootCount  = 0;
String   g_lfType     = "none";
bool     g_lfOk       = false;
uint32_t g_lfEpoch    = 0;
uint8_t  g_servoPin   = 13;
}

namespace AppConfig {

void begin() {
  prefs.begin(NS, false);                       // read/write namespace
  g_deviceName = prefs.getString("devName", "shutter-hub");
  g_lfType     = prefs.getString("lfType", "none");
  g_lfOk       = prefs.getBool("lfOk", false);
  g_lfEpoch    = prefs.getUInt("lfEpoch", 0);
  g_servoPin   = prefs.getUChar("servoPin", 13);
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

String   lastFlashType()  { return g_lfType; }
bool     lastFlashOk()    { return g_lfOk; }
uint32_t lastFlashEpoch() { return g_lfEpoch; }

void recordFlash(const String &type, bool ok, uint32_t epoch) {
  g_lfType = type; g_lfOk = ok; g_lfEpoch = epoch;
  prefs.putString("lfType", type);
  prefs.putBool("lfOk", ok);
  prefs.putUInt("lfEpoch", epoch);
}

}  // namespace AppConfig
