#include "HomeKit.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include "Shutters.h"
#include "ServoController.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "HomeSpan.h"

// Phase 5 (v0.5.0): a HomeSpan bridge that mirrors the MQTT cover integration —
// one Apple Home "Window Covering" accessory per configured shutter, driving the
// same ServoController slots and reusing the same µs↔position maths (Mqtt.cpp).
// See HomeKit.h for the coexistence contract with WiFiManager / mDNS / the web server.

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

namespace {
bool          g_started      = false;   // bridge actually begun (enabled + set up)
volatile bool g_paired       = false;   // at least one controller paired (pair callback)
volatile bool g_resetPending = false;   // web UI asked to erase pairings
unsigned long g_resetAt      = 0;       // when to run it (lets the HTTP response flush)

// Strings handed to HomeSpan must outlive begin() — keep our own copies.
String g_bridgeName, g_code, g_ssid, g_psk, g_host;
String g_names[Shutters::MAX];

// ---- Position maths — same model as the MQTT cover (0 % = closed unless inverted).
// Kept local, mirroring Mqtt.cpp's copy, so HomeKit and MQTT report identically.
//
// MVP (owner request): a shutter must be *operable* from Apple Home even before it's
// calibrated — HomeKit is for driving blinds, not configuring them. So when an edge
// isn't calibrated we substitute the servo's pulse envelope (min = closed, max = open),
// giving HomeKit a usable 0–100 % range out of the box. `calibrated` records the real
// state (for logging); `cal` just means "operable" (the two ends differ).
struct ShInfo { uint8_t slot; bool cal; bool calibrated; bool inv; int openUs; int closedUs; };

ShInfo shInfo(int i) {
  ShInfo s;
  s.slot       = ServoController::usesPca() ? Shutters::channelAt(i) : 0;
  s.inv        = Shutters::invertedAt(i);
  String id    = Shutters::idAt(i);
  int op       = Shutters::edgeUs(id, true);
  int cl       = Shutters::edgeUs(id, false);
  s.calibrated = op != Shutters::UNSET && cl != Shutters::UNSET && op != cl;
  s.openUs     = (op != Shutters::UNSET) ? op : ServoController::maxUs();
  s.closedUs   = (cl != Shutters::UNSET) ? cl : ServoController::minUs();
  s.cal        = s.openUs != s.closedUs;      // operable as long as the ends differ
  return s;
}

int usToPct(const ShInfo &s, int us) {
  if (!s.cal) return 0;
  int pct = (int)lroundf(100.0f * (us - s.closedUs) / (float)(s.openUs - s.closedUs));
  if (s.inv) pct = 100 - pct;
  return constrain(pct, 0, 100);
}

int pctToUs(const ShInfo &s, int pct) {
  pct = constrain(pct, 0, 100);
  if (s.inv) pct = 100 - pct;
  return s.closedUs + (int)lroundf((s.openUs - s.closedUs) * pct / 100.0f);
}

// ---- One HomeKit Window Covering, bound to a shutter index --------------------
// HomeKit decides opening/closing/stopped purely by comparing CurrentPosition to
// TargetPosition, so there's no PositionState to maintain: update() drives the servo
// on a Home-app change; loop() streams the live position back and, once the slot has
// settled, syncs the target to the real position so moves made elsewhere (MQTT, the
// web UI) are reflected in Apple Home too.
struct DevShutter : Service::WindowCovering {
  int idx;
  uint8_t slot;
  SpanCharacteristic *current;
  SpanCharacteristic *target;

  DevShutter(int i) : Service::WindowCovering() {
    idx = i;
    ShInfo s = shInfo(i);
    slot = s.slot;
    int pct = usToPct(s, ServoController::slotUs(slot));
    current = new Characteristic::CurrentPosition(pct);
    target  = new Characteristic::TargetPosition(pct);
  }

  boolean update() override {
    ShInfo s = shInfo(idx);
    int tp   = target->getNewVal();
    if (!s.cal) {                         // degenerate (min==max) — nothing to drive
      LOGW("homekit", "%s: target %d%% ignored — no usable travel range",
           Shutters::idAt(idx).c_str(), tp);
      return false;                       // reject: Home app reverts the tile
    }
    int us = pctToUs(s, tp);
    LOGI("homekit", "%s: target %d%% -> %d us%s", Shutters::idAt(idx).c_str(), tp, us,
         s.calibrated ? "" : " (default envelope — not calibrated)");
    ServoController::moveSlotUs(slot, us);
    return true;
  }

  void loop() override {
    ShInfo s = shInfo(idx);
    int pct  = usToPct(s, ServoController::slotUs(slot));
    if (pct != current->getVal()) current->setVal(pct);
    // Once the servo has stopped, make HomeKit's target match reality so an
    // external move (MQTT/web UI) doesn't leave the tile showing a phantom motion.
    if (!ServoController::slotMoving(slot) && target->getVal() != pct) target->setVal(pct);
  }
};

// ---- HomeSpan callbacks ------------------------------------------------------
void onConnection(int count) {
  LOGI("homekit", "HomeSpan network %s", count ? "connected" : "disconnected");
}
void onPair(boolean isPaired) {
  g_paired = isPaired;
  LOGI("homekit", "%s", isPaired ? "paired with a controller" : "no controllers paired");
}
void onController() {
  LOGI("homekit", "controller list changed (%d paired)", HomeKit::controllers());
}
}  // namespace

namespace HomeKit {

void begin() {
  if (!AppConfig::hkEnabled()) { LOGI("homekit", "disabled"); return; }

  g_bridgeName = AppConfig::hkBridgeName();
  g_code       = AppConfig::hkSetupCode();
  g_host       = AppConfig::deviceName();
  g_ssid       = WiFi.SSID();
  g_psk        = WiFi.psk();

  LOGI("homekit", "starting HomeSpan bridge '%s' (setup %s) — HAP on port 1201",
       g_bridgeName.c_str(), g_code.c_str());

  homeSpan.setPortNum(1201);              // leave port 80 to the async web server
  homeSpan.setQRID("SHUT");               // must match the web UI's X-HM:// QR payload
  homeSpan.setHostNameSuffix("");         // hostName == deviceName -> keep <name>.local
  homeSpan.setSketchVersion(FW_VERSION);
  homeSpan.setLogLevel(0);
  homeSpan.setSerialInputDisable(true);   // don't consume the serial console we log to
  // WiFiManager already has us connected before HomeSpan polls, so HomeSpan's checkConnect()
  // sees WL_CONNECTED and never calls WiFi.begin() itself — no startup blip. We still hand it
  // the credentials so it can reconnect on its own if the link later drops.
  homeSpan.setWifiCallbackAll(onConnection);   // (1.9.x name for setConnectionCallback)
  homeSpan.setPairCallback(onPair);
  homeSpan.setControllerCallback(onController);

  homeSpan.begin(Category::Bridges, g_bridgeName.c_str(), g_host.c_str(), "ESP32 Shutter Hub");

  // After begin() the NVS handles are open: safe to set the code (progCall=false so a
  // bad code just logs instead of halting the CPU) and persist the WiFi credentials.
  homeSpan.setPairingCode(g_code.c_str(), false);
  homeSpan.setWifiCredentials(g_ssid.c_str(), g_psk.c_str());

  // Bridge accessory (AID 1).
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name(g_bridgeName.c_str());

  // One Window Covering accessory per configured shutter.
  int n = min(Shutters::count(), (int)Shutters::MAX);
  for (int i = 0; i < n; i++) {
    g_names[i] = Shutters::nameAt(i);
    new SpanAccessory();
      new Service::AccessoryInformation();
        new Characteristic::Identify();
        new Characteristic::Name(g_names[i].c_str());
      new DevShutter(i);
    ShInfo s = shInfo(i);
    LOGI("homekit", "  accessory '%s' -> slot %u, %s",
         g_names[i].c_str(), s.slot,
         s.calibrated ? "calibrated" : "default envelope (operable, calibrate for accuracy)");
  }
  LOGI("homekit", "bridge up — %d shutter accessory(ies)", n);

  // Echo the ACTIVE pairing code (the one baked into the running bridge this boot) so it
  // can be verified from the web Logs page against what the tab shows — HomeSpan's own
  // pairing log only reaches the USB serial console. If these ever differ, reboot: the
  // code is applied at begin(), so a code changed in the tab needs a restart to take hold.
  const char *c = g_code.c_str();
  LOGI("homekit", "PAIR WITH THIS CODE: %.3s-%.2s-%.3s (bridge '%s', QR id SHUT on port 1201)",
       c, c + 3, c + 5, g_bridgeName.c_str());

  // HomeSpan re-ran MDNS.begin(hostName); re-add the web UI's http service under it.
  MDNS.addService("http", "tcp", 80);

  g_started = true;
}

void loop() {
  if (!g_started) return;
  homeSpan.poll();
  if (g_resetPending && millis() >= g_resetAt) {
    g_resetPending = false;
    LOGW("homekit", "erasing HomeKit pairing data and restarting");
    homeSpan.processSerialCommand("H");   // erases pairing data, then reboots
  }
}

bool running() { return g_started; }
bool paired()  { return g_paired; }

int controllers() {
  if (!g_started) return 0;
  int c = 0;
  for (auto it = homeSpan.controllerListBegin(); it != homeSpan.controllerListEnd(); ++it) c++;
  return c;
}

bool resetPairings() {
  if (!g_started) return false;
  g_resetPending = true;
  g_resetAt = millis() + 400;             // let the HTTP response flush before the reboot
  return true;
}

}  // namespace HomeKit
