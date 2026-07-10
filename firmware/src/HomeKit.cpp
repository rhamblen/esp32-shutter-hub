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

// [dbg v0.7.0] Breadcrumb hook, called from an INSTRUMENTED copy of HomeSpan's checkConnect()
// (firmware/.pio/libdeps/<env>/HomeSpan/src/HomeSpan.cpp) to localise exactly where its post-connect
// init stalls — the poll task reaches "Device not yet Paired" but never adds _hap or opens 1201.
// Logged at WARN so the crumbs stand out. TEMPORARY: revert the library edits + this hook once found.
void hsCrumb(const char *m) { LOGW("homekit", "[hs] %s", m); }

namespace {
bool          g_started      = false;   // bridge actually begun (enabled + set up)
volatile bool g_paired       = false;   // at least one controller paired (pair callback)
volatile bool g_resetPending = false;   // web UI asked to erase pairings
unsigned long g_resetAt      = 0;       // when to run it (lets the HTTP response flush)
unsigned long g_dbgAt        = 0;       // [dbg v0.7.0] when to emit the one-shot bridge summary
bool          g_dbgDone      = false;   // [dbg v0.7.0] summary already logged?
volatile bool g_hapUp        = false;   // HomeSpan finished post-connect init (connect callback fired)
unsigned long g_hapWatchAt   = 0;       // when to WARN if the HAP server never came up
bool          g_hapWarned    = false;   // stall WARN already emitted?

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
  if (count) g_hapUp = true;            // reached the END of HomeSpan's post-connect init (HAP up)
  LOGI("homekit", "HomeSpan network %s", count ? "connected" : "disconnected");
}
void onPair(boolean isPaired) {
  g_paired = isPaired;
  LOGI("homekit", "%s", isPaired ? "paired with a controller" : "no controllers paired");
}
void onController() {
  LOGI("homekit", "controller list changed (%d paired)", HomeKit::controllers());
}
// [dbg v0.7.0] Stream HomeSpan's own state machine to the web Logs page so we can see the
// bridge progress without a USB serial cable. The states that matter for the "device won't
// show up in the Home app" hunt: HS_WIFI_CONNECTING -> HS_PAIRING_NEEDED means the bridge is
// up, mDNS is announced and it's *discoverable*; HS_PAIRED means a controller got through.
// Remove this callback (and its setStatusCallback registration) once pairing is confirmed.
void onStatus(HS_STATUS s) {
  LOGD("homekit", "[dbg] HomeSpan status -> %s", homeSpan.statusString(s));
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
  homeSpan.setStatusCallback(onStatus);        // [dbg v0.7.0] mirror HomeSpan state to web Logs

  // HomeSpan is the SOLE mDNS owner when HomeKit is on: WebUI::begin() deliberately skips its
  // own MDNS.begin() in that case (two initialisers of the one shared responder collided and left
  // _hap._tcp unannounced — the Home app then discovers nothing; the working HomeKey-ESP32 build
  // avoids it the same way). So homeSpan.begin() below performs the one and only mDNS init here;
  // we re-add the web UI's _http service under it afterwards.
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

  // mDNS is already up: WebUI::begin() initialised the shared responder (hostname + _http._tcp)
  // on the main thread before we got here. HomeSpan's checkConnect() will ADD its _hap._tcp
  // service to that running responder. We deliberately do NOT touch mDNS from this side any more —
  // the old MDNS.addService() here ran before the responder existed (it logged FAILED) and the
  // whole "let HomeSpan own mDNS" scheme hung HomeSpan's poll task at mdns_init() (see WebUI.cpp).

  // Run HomeSpan on its OWN FreeRTOS task (autoPoll) — NOT from the Arduino loop(): HAP/pairing
  // crypto can hold the CPU for long stretches and, on the shared main loop, that starved servo
  // slewing + MQTT (servos/HA froze whenever the bridge was on). It runs on CORE 1 (with the Arduino
  // loop), NOT core 0: HomeSpan's post-connect init calls into the ESP-IDF mDNS and lwIP service
  // tasks, which live on core 0 — pinning our poll task to core 0 as well made it BLOCK waiting on
  // those same-core tasks, so init never finished (no _hap._tcp, HAP server never opened on 1201,
  // connect callback never fired — v0.7.0 diagnosis). On core 1 the core-0 network tasks run freely
  // and init completes; preemptive scheduling keeps servos/MQTT responsive. 16 KB stack gives the
  // SRP big-number math headroom during pairing.
  homeSpan.autoPoll(16384, 1, 1);        // stackSize, priority, cpu core (core 1 — see above)

  g_started    = true;
  g_dbgAt      = millis() + 8000;        // [dbg v0.7.0] summarise the bridge ~8 s after boot
  g_hapWatchAt = millis() + 10000;       // WARN if HAP init hasn't completed by 10 s (see loop())
}

void loop() {
  // homeSpan.poll() now runs on its own task (autoPoll, see begin()) — the main loop must NOT
  // call it too. This only services the deferred pairing reset.
  if (!g_started) return;

  // [dbg v0.7.0] One-shot bridge summary on the web Logs page ~8 s after boot. Confirms the
  // bridge is still up, flags an *unexpected* existing pairing (controllers>0 while the Home app
  // can't see it = stale sf=0 pairing, fix with the Reset Pairings button), and prints the name
  // to ping. If the Home app shows nothing, run `ping <name>.local` from a PC: resolves = mDNS
  // is live and the issue is elsewhere; fails = _hap._tcp isn't on the wire.
  if (!g_dbgDone && millis() >= g_dbgAt) {
    g_dbgDone = true;
    LOGD("homekit", "[dbg] bridge summary: running=%d paired=%d controllers=%d hapUp=%d host=%s.local port=1201 freeHeap=%u",
         (int)g_started, (int)g_paired, controllers(), (int)g_hapUp, g_host.c_str(), (unsigned)ESP.getFreeHeap());
    LOGD("homekit", "[dbg] if the Home app can't see it, from a PC run:  ping %s.local", g_host.c_str());
  }

  // Health watchdog (WARN): HomeSpan's poll task should finish its network init within a few seconds
  // of WiFi being up. If the connect callback never fired, the bridge is NOT advertising _hap._tcp
  // and nothing is listening on 1201 — the Home app can neither discover nor pair. Say so loudly,
  // once, rather than failing silently.
  if (!g_hapWarned && !g_hapUp && millis() >= g_hapWatchAt) {
    g_hapWarned = true;
    LOGW("homekit", "HAP server did not come up within 10s — HomeSpan poll task stalled during "
                    "network init; bridge NOT discoverable, port 1201 closed");
  }

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
