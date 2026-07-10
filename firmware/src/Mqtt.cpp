#include "Mqtt.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include "Shutters.h"
#include "ServoController.h"
#include "LightSensor.h"
#include "SolarLogic.h"
#include <WiFi.h>
#include <PubSubClient.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

// Home Assistant MQTT integration (Phase 4, ADR 0005/0006/0010): hub availability +
// diagnostics, one `cover` + six `button` entities per shutter via discovery, and the
// per-shutter command handlers wired to ServoController slots. Every publish/receive
// is logged through the LOG* macros so it streams live to the Logs page.

namespace {
WiFiClient   net;
PubSubClient client(net);

bool          g_enabled   = false;
String        g_host, g_user, g_pass, g_clientId, g_base, g_node;
uint16_t      g_port      = 1883;
bool          g_haDisc    = true;
String        g_state     = "disabled";
unsigned long g_nextTry   = 0;         // next reconnect attempt (millis)
unsigned long g_nextStat  = 0;         // next periodic hub-state publish (millis)
unsigned long g_nextShPub = 0;         // next shutter position/state publish check (millis)
unsigned long g_nextSolPub = 0;        // next solar lux/state publish check (millis)
volatile bool g_shDirty   = false;     // shutter config mutated — refresh discovery/state
                                       // (set from the web task, consumed in loop())
volatile bool g_solDirty  = false;     // solar config mutated — re-publish the solar topics

const int      JOG_STEP_US    = 25;    // one HA jog press (ADR 0005 "incremental jog")
const uint32_t SHUTTER_PUB_MS = 250;   // shutter position/state publish cadence while moving
const uint32_t SOLAR_PUB_MS   = 5000;  // solar lux/state publish cadence
const int      SOLAR_LUX_EPS  = 25;    // don't re-publish lux for jitter smaller than this

int    g_lastLux      = -1;            // last published lux / solar state (publish on change only)
String g_lastSolState = "";

// Last published per shutter index, so state topics only publish on change.
int    g_lastPct[Shutters::MAX];
String g_lastState[Shutters::MAX];
// Shutter ids with retained discovery/state on the broker — lets a deleted shutter's
// entities be removed (empty retained publish) on the next refresh.
String g_discIds[Shutters::MAX];
int    g_discCount = 0;

String statusTopic() { return g_base + "/status"; }
String stateTopic()  { return g_base + "/state"; }
String cmdTopic()    { return g_base + "/cmd"; }
String coverTopic(const String &id, const char *leaf) {
  return g_base + "/cover/" + id + "/" + leaf;
}
String solarTopic(const String &leaf) { return g_base + "/solar/" + leaf; }

// Sanitise a string into an MQTT/HA-safe id (alnum + underscore).
String slug(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    o += (isalnum((int)c) ? c : '_');
  }
  return o;
}

// escape a string for embedding in JSON
String jesc(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o;
}

// Shared HA device block so all entities group under one device.
String deviceBlock() {
  return String("\"dev\":{\"ids\":[\"") + g_node + "\"],\"name\":\"" + AppConfig::deviceName() +
         "\",\"mdl\":\"ESP32 Shutter Hub\",\"mf\":\"rhamblen\",\"sw\":\"" FW_VERSION "\"}";
}

void publish(const String &topic, const String &payload, bool retain) {
  bool ok = client.publish(topic.c_str(), payload.c_str(), retain);
  LOGD("mqtt", "tx %s (%u B)%s%s", topic.c_str(), (unsigned)payload.length(),
       retain ? " [retained]" : "", ok ? "" : " FAILED");
}

// ---- Shutter helpers ---------------------------------------------------------

// Everything the handlers need about one shutter, fetched once per event.
struct ShInfo {
  String id;
  int    slot;        // physical output: PCA9685 channel, or the one direct-GPIO servo
  bool   cal;         // both edges known and distinct (position % is computable)
  bool   inv;         // reported scale inverted (0 % = open)
  int    openUs, closedUs;
};

ShInfo shInfo(int i) {
  ShInfo s;
  s.id       = Shutters::idAt(i);
  s.slot     = ServoController::usesPca() ? Shutters::channelAt(i) : 0;
  s.inv      = Shutters::invertedAt(i);
  s.openUs   = Shutters::edgeUs(s.id, true);
  s.closedUs = Shutters::edgeUs(s.id, false);
  s.cal      = s.openUs != Shutters::UNSET && s.closedUs != Shutters::UNSET &&
               s.openUs != s.closedUs;
  return s;
}

// µs → reported position 0–100 (0 = closed unless the scale is inverted). An
// uncalibrated shutter reports 0/closed — the assembly-home convention (ADR 0009).
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

// HA cover state, consistent with the reported position scale.
String coverState(const ShInfo &s, int us, int tgt, bool moving, int pct) {
  if (moving) {
    bool openHigher = s.cal ? (s.openUs > s.closedUs) : true;  // uncal: home = closed at MIN (ADR 0009)
    bool towardOpen = (tgt > us) == openHigher;
    if (s.inv) towardOpen = !towardOpen;
    return towardOpen ? "opening" : "closing";
  }
  if (pct <= 0)   return "closed";
  if (pct >= 100) return "open";
  return "stopped";
}

void resetStateCache() {
  for (int i = 0; i < Shutters::MAX; i++) { g_lastPct[i] = -1; g_lastState[i] = ""; }
}

// Publish position/state for every shutter (retained) — on change, or all if forced.
void publishShutterStates(bool force) {
  int n = min(Shutters::count(), (int)Shutters::MAX);
  for (int i = 0; i < n; i++) {
    ShInfo s    = shInfo(i);
    int  us     = ServoController::slotUs(s.slot);
    int  tgt    = ServoController::slotTargetUs(s.slot);
    bool moving = ServoController::slotMoving(s.slot);
    int  pct    = usToPct(s, us);
    String st   = coverState(s, us, tgt, moving, pct);
    if (force || pct != g_lastPct[i]) {
      publish(coverTopic(s.id, "position"), String(pct), true);
      g_lastPct[i] = pct;
    }
    if (force || st != g_lastState[i]) {
      publish(coverTopic(s.id, "state"), st, true);
      g_lastState[i] = st;
    }
  }
}

// ---- Home Assistant discovery --------------------------------------------------

// One HA discovery sensor reading a field out of the hub's JSON state topic.
void publishSensor(const char *object, const char *name, const char *field,
                   const char *devClass, const char *unit) {
  String cfgTopic = "homeassistant/sensor/" + g_node + "/" + object + "/config";
  String p = "{\"name\":\"" + String(name) + "\",\"uniq_id\":\"" + g_node + "_" + object + "\",";
  p += "\"stat_t\":\"" + stateTopic() + "\",\"avty_t\":\"" + statusTopic() + "\",";
  p += "\"val_tpl\":\"{{ value_json." + String(field) + " }}\",";
  if (devClass && *devClass) p += "\"dev_cla\":\"" + String(devClass) + "\",";
  if (unit && *unit)         p += "\"unit_of_meas\":\"" + String(unit) + "\",";
  p += "\"ent_cat\":\"diagnostic\"," + deviceBlock() + "}";
  publish(cfgTopic, p, true);
}

// The six per-shutter buttons (ADR 0005): jog, recall presets, save presets.
// Icons are declared device-side so the entities look right everywhere in HA.
struct BtnDef { const char *object; const char *label; const char *payload; const char *icon; bool config; };
const BtnDef BTNS[] = {
  {"jog_open",        "Jog open",      "jog_open",        "mdi:chevron-double-up",       false},
  {"jog_close",       "Jog close",     "jog_close",       "mdi:chevron-double-down",     false},
  {"recall_daylight", "Daylight",      "recall:daylight", "mdi:window-shutter-settings", false},
  {"recall_privacy",  "Privacy",       "recall:privacy",  "mdi:window-shutter-settings", false},
  {"save_daylight",   "Save daylight", "save:daylight",   "mdi:content-save-cog",        true},
  {"save_privacy",    "Save privacy",  "save:privacy",    "mdi:content-save-cog",        true},
};

void publishCoverDiscovery(const String &id, const String &name) {
  String cfg = "homeassistant/cover/" + g_node + "/" + id + "/config";
  String b   = g_base + "/cover/" + id;
  String p = "{\"name\":\"" + jesc(name) + "\",\"uniq_id\":\"" + g_node + "_" + id + "\",";
  p += "\"dev_cla\":\"shutter\",";
  p += "\"cmd_t\":\"" + b + "/set\",";
  p += "\"pos_t\":\"" + b + "/position\",";
  p += "\"set_pos_t\":\"" + b + "/position/set\",";
  p += "\"stat_t\":\"" + b + "/state\",";
  p += "\"avty_t\":\"" + statusTopic() + "\",";
  p += deviceBlock() + "}";
  publish(cfg, p, true);
}

void publishButtonDiscovery(const String &id, const String &shName, const BtnDef &b) {
  String cfg = "homeassistant/button/" + g_node + "/" + id + "_" + b.object + "/config";
  String p = "{\"name\":\"" + jesc(shName) + " " + b.label + "\",";
  p += "\"uniq_id\":\"" + g_node + "_" + id + "_" + b.object + "\",";
  p += "\"cmd_t\":\"" + g_base + "/cover/" + id + "/cmd\",";
  p += "\"pl_prs\":\"" + String(b.payload) + "\",";
  p += "\"ic\":\"" + String(b.icon) + "\",";
  p += "\"avty_t\":\"" + statusTopic() + "\",";
  if (b.config) p += "\"ent_cat\":\"config\",";
  p += deviceBlock() + "}";
  publish(cfg, p, true);
}

// Delete a shutter's HA entities + retained state (empty retained publishes).
void removeShutterDiscovery(const String &id) {
  publish("homeassistant/cover/" + g_node + "/" + id + "/config", "", true);
  for (const BtnDef &b : BTNS)
    publish("homeassistant/button/" + g_node + "/" + id + "_" + b.object + "/config", "", true);
  publish(coverTopic(id, "position"), "", true);
  publish(coverTopic(id, "state"), "", true);
}

// (Re)publish per-shutter discovery; remove entities of shutters that no longer exist.
void publishShutterDiscovery() {
  for (int k = 0; k < g_discCount; k++)
    if (!Shutters::exists(g_discIds[k])) {
      LOGI("mqtt", "removing HA entities for deleted shutter '%s'", g_discIds[k].c_str());
      removeShutterDiscovery(g_discIds[k]);
    }
  g_discCount = 0;
  if (!g_haDisc) return;
  int n = min(Shutters::count(), (int)Shutters::MAX);
  for (int i = 0; i < n; i++) {
    String id   = Shutters::idAt(i);
    String name = Shutters::nameAt(i);
    publishCoverDiscovery(id, name);
    for (const BtnDef &b : BTNS) publishButtonDiscovery(id, name, b);
    g_discIds[i] = id;
    g_discCount  = i + 1;
  }
  if (n) LOGI("mqtt", "HA discovery: %d cover(s) + %d button(s) published", n, n * 6);
}

void publishDiscovery() {
  if (!g_haDisc) { LOGI("mqtt", "HA discovery disabled — skipping"); return; }
  LOGI("mqtt", "publishing Home Assistant discovery");
  publishSensor("rssi",   "WiFi signal", "rssi",   "signal_strength", "dBm");
  publishSensor("uptime", "Uptime",      "uptime", "duration",        "s");
}

void publishState() {
  String s = "{\"rssi\":" + String(WiFi.RSSI()) +
             ",\"uptime\":" + String(millis() / 1000UL) +
             ",\"heap\":" + String(ESP.getFreeHeap()) + "}";
  publish(stateTopic(), s, false);
}

// ---- Solar heat protection (Phase 6) -------------------------------------------

// A writable lux threshold as an HA `number` — this is the write-back hook a future
// HA-side calibration card uses to push a recommended threshold onto the hub.
void publishSolarNumber(const char *object, const char *name, const char *leaf, const char *icon) {
  String cfg = "homeassistant/number/" + g_node + "/" + object + "/config";
  String p = "{\"name\":\"" + String(name) + "\",\"uniq_id\":\"" + g_node + "_" + object + "\",";
  p += "\"cmd_t\":\"" + solarTopic(String(leaf) + "/set") + "\",";
  p += "\"stat_t\":\"" + solarTopic(leaf) + "\",";
  p += "\"min\":0,\"max\":130000,\"step\":500,\"unit_of_meas\":\"lx\",\"mode\":\"box\",";
  p += "\"ic\":\"" + String(icon) + "\",\"ent_cat\":\"config\",";
  p += "\"avty_t\":\"" + statusTopic() + "\"," + deviceBlock() + "}";
  publish(cfg, p, true);
}

void publishSolarDiscovery() {
  if (!g_haDisc) return;
  {   // live light level
    String cfg = "homeassistant/sensor/" + g_node + "/solar_lux/config";
    String p = "{\"name\":\"Light level\",\"uniq_id\":\"" + g_node + "_solar_lux\",";
    p += "\"stat_t\":\"" + solarTopic("lux") + "\",\"avty_t\":\"" + statusTopic() + "\",";
    p += "\"dev_cla\":\"illuminance\",\"unit_of_meas\":\"lx\",\"stat_cla\":\"measurement\",";
    p += deviceBlock() + "}";
    publish(cfg, p, true);
  }
  {   // state machine position
    String cfg = "homeassistant/sensor/" + g_node + "/solar_state/config";
    String p = "{\"name\":\"Solar state\",\"uniq_id\":\"" + g_node + "_solar_state\",";
    p += "\"stat_t\":\"" + solarTopic("state") + "\",\"avty_t\":\"" + statusTopic() + "\",";
    p += "\"ic\":\"mdi:weather-sunny\"," + deviceBlock() + "}";
    publish(cfg, p, true);
  }
  {   // automation on/off
    String cfg = "homeassistant/switch/" + g_node + "/solar_enable/config";
    String p = "{\"name\":\"Solar automation\",\"uniq_id\":\"" + g_node + "_solar_enable\",";
    p += "\"cmd_t\":\"" + solarTopic("enable/set") + "\",\"stat_t\":\"" + solarTopic("enable") + "\",";
    p += "\"pl_on\":\"ON\",\"pl_off\":\"OFF\",\"avty_t\":\"" + statusTopic() + "\",";
    p += "\"ic\":\"mdi:sun-thermometer\"," + deviceBlock() + "}";
    publish(cfg, p, true);
  }
  publishSolarNumber("solar_trip_lux",  "Solar trip lux",  "trip_lux",  "mdi:brightness-7");
  publishSolarNumber("solar_clear_lux", "Solar clear lux", "clear_lux", "mdi:brightness-4");
  LOGI("mqtt", "HA discovery: solar sensor/state/switch + 2 threshold numbers published");
}

// lux + state publish on change (lux ignores sub-EPS jitter); `force` also echoes config.
void publishSolarStates(bool force) {
  int    lux = (int)lroundf(LightSensor::lux());
  String st  = SolarLogic::stateText();
  if (force || abs(lux - g_lastLux) >= SOLAR_LUX_EPS) {
    publish(solarTopic("lux"), String(lux), true);
    g_lastLux = lux;
  }
  if (force || st != g_lastSolState) {
    publish(solarTopic("state"), st, true);
    g_lastSolState = st;
  }
  if (force) {
    publish(solarTopic("enable"),    AppConfig::solarEnabled() ? "ON" : "OFF", true);
    publish(solarTopic("trip_lux"),  String(AppConfig::solarTripLux()),  true);
    publish(solarTopic("clear_lux"), String(AppConfig::solarClearLux()), true);
  }
}

// <base>/solar/{enable,trip_lux,clear_lux}/set. Thresholds must keep clear < trip, or the
// hysteresis inverts and the machine oscillates — reject rather than accept a bad config.
void handleSolar(const String &leaf, const String &msg) {
  bool     en    = AppConfig::solarEnabled();
  uint32_t trip  = AppConfig::solarTripLux();
  uint32_t clear = AppConfig::solarClearLux();

  if (leaf == "enable/set") {
    en = (msg == "ON" || msg == "on" || msg == "1");
    LOGI("mqtt", "solar automation → %s", en ? "on" : "off");
  } else if (leaf == "trip_lux/set" || leaf == "clear_lux/set") {
    long v = msg.toInt();
    if (v < 0 || v > 130000) { LOGW("mqtt", "solar: lux %ld out of range", v); return; }
    if (leaf == "trip_lux/set") trip = (uint32_t)v; else clear = (uint32_t)v;
    if (clear >= trip) {
      LOGW("mqtt", "solar: rejected — clear (%lu) must stay below trip (%lu)",
           (unsigned long)clear, (unsigned long)trip);
      return;
    }
    LOGI("mqtt", "solar thresholds → trip %lu lx / clear %lu lx",
         (unsigned long)trip, (unsigned long)clear);
  } else {
    LOGW("mqtt", "solar: unknown topic leaf '%s'", leaf.c_str());
    return;
  }
  AppConfig::setSolar(en, trip, AppConfig::solarTripSecs(), clear,
                      AppConfig::solarClearSecs(), AppConfig::solarBrightTarget(),
                      AppConfig::solarClearTarget());
  publishSolarStates(true);
}

// ---- Command handling ----------------------------------------------------------

void handleCover(const String &id, const String &sub, const String &msg) {
  int i = Shutters::find(id);
  if (i < 0) { LOGW("mqtt", "command for unknown shutter '%s'", id.c_str()); return; }
  ShInfo s = shInfo(i);

  if (sub == "set") {                        // HA cover verbs: OPEN | CLOSE | STOP
    if (msg == "OPEN" || msg == "CLOSE") {
      bool open = (msg == "OPEN");
      int us = open ? s.openUs : s.closedUs;
      if (us == Shutters::UNSET) {
        LOGW("mqtt", "%s: %s ignored — %s edge not calibrated", id.c_str(), msg.c_str(),
             open ? "open" : "closed");
        return;
      }
      LOGI("mqtt", "%s: %s → %d µs", id.c_str(), msg.c_str(), us);
      ServoController::moveSlotUs(s.slot, us);
      SolarLogic::notifyManualMove(id);
    } else if (msg == "STOP") {
      LOGI("mqtt", "%s: STOP", id.c_str());
      ServoController::stopSlot(s.slot);
    } else {
      LOGW("mqtt", "%s: unknown set payload '%s'", id.c_str(), msg.c_str());
    }
    return;
  }

  if (sub == "position/set") {               // HA position slider: 0–100
    if (!s.cal) { LOGW("mqtt", "%s: position ignored — not calibrated", id.c_str()); return; }
    int pct = constrain((int)msg.toInt(), 0, 100);
    int us  = pctToUs(s, pct);
    LOGI("mqtt", "%s: position %d%% → %d µs", id.c_str(), pct, us);
    ServoController::moveSlotUs(s.slot, us);
    SolarLogic::notifyManualMove(id);
    return;
  }

  if (sub == "cmd") {                        // hub-specific verbs (ADR 0005)
    if (msg == "jog_open" || msg == "jog_close") {
      bool open       = (msg == "jog_open");
      bool openHigher = s.cal ? (s.openUs > s.closedUs) : true;
      int  dir        = (open == openHigher) ? +1 : -1;
      int  tgt        = ServoController::slotTargetUs(s.slot) + dir * JOG_STEP_US;
      if (s.cal)                             // never jog past the calibrated endpoints
        tgt = constrain(tgt, min(s.openUs, s.closedUs), max(s.openUs, s.closedUs));
      LOGI("mqtt", "%s: %s → %d µs", id.c_str(), msg.c_str(), tgt);
      ServoController::moveSlotUs(s.slot, tgt);
      SolarLogic::notifyManualMove(id);
    } else if (msg.startsWith("recall:")) {
      String fav = msg.substring(7);
      int us = Shutters::UNSET;
      if      (fav == "daylight") us = Shutters::favUs(id, false);
      else if (fav == "privacy")  us = Shutters::favUs(id, true);
      else { LOGW("mqtt", "%s: unknown recall '%s'", id.c_str(), fav.c_str()); return; }
      if (us == Shutters::UNSET) {
        LOGW("mqtt", "%s: recall:%s ignored — favourite not saved yet", id.c_str(), fav.c_str());
        return;
      }
      LOGI("mqtt", "%s: recall:%s → %d µs", id.c_str(), fav.c_str(), us);
      ServoController::moveSlotUs(s.slot, us);
      SolarLogic::notifyManualMove(id);
    } else if (msg.startsWith("save:")) {
      String fav = msg.substring(5);
      if (fav != "daylight" && fav != "privacy") {
        LOGW("mqtt", "%s: unknown save '%s'", id.c_str(), fav.c_str()); return;
      }
      int us = ServoController::slotUs(s.slot);
      Shutters::saveFav(id, fav == "privacy", us);
      LOGI("mqtt", "%s: saved %s = %d µs", id.c_str(), fav.c_str(), us);
    } else {
      LOGW("mqtt", "%s: unknown cmd '%s'", id.c_str(), msg.c_str());
    }
  }
}

void onMessage(char *topic, byte *payload, unsigned int len) {
  String t(topic), msg;
  for (unsigned int k = 0; k < len && k < 180; k++) msg += (char)payload[k];
  LOGI("mqtt", "rx %s: %s", topic, msg.c_str());
  String pfx = g_base + "/cover/";
  if (t.startsWith(pfx)) {
    String rest = t.substring(pfx.length());       // "<id>/set" | "<id>/position/set" | "<id>/cmd"
    int sl = rest.indexOf('/');
    if (sl > 0) handleCover(rest.substring(0, sl), rest.substring(sl + 1), msg);
    return;
  }
  String spfx = g_base + "/solar/";
  if (t.startsWith(spfx)) { handleSolar(t.substring(spfx.length()), msg); return; }
  // <base>/cmd — hub-wide commands reserved; logged only for now.
}

void loadConfig() {
  g_enabled  = AppConfig::mqttEnabled();
  g_host     = AppConfig::mqttHost();
  g_port     = AppConfig::mqttPort();
  g_user     = AppConfig::mqttUser();
  g_pass     = AppConfig::mqttPass();
  g_clientId = AppConfig::mqttClientId();
  g_base     = AppConfig::mqttBaseTopic();
  g_haDisc   = AppConfig::mqttHaDiscovery();
  g_node     = slug(g_clientId);
}

bool tryConnect() {
  if (g_host.isEmpty()) { g_state = "error: no broker set"; return false; }
  g_state = "connecting";
  LOGI("mqtt", "connecting to %s:%u as %s", g_host.c_str(), g_port, g_clientId.c_str());
  client.setServer(g_host.c_str(), g_port);
  client.setBufferSize(1024);            // HA discovery payloads exceed the 256B default
  client.setCallback(onMessage);

  const char *u = g_user.length() ? g_user.c_str() : nullptr;
  const char *p = g_pass.length() ? g_pass.c_str() : nullptr;
  // LWT: broker marks us offline if the connection drops.
  bool ok = client.connect(g_clientId.c_str(), u, p,
                           statusTopic().c_str(), 0, true, "offline");
  if (!ok) {
    g_state = "error: rc=" + String(client.state());
    LOGW("mqtt", "connect failed, %s", g_state.c_str());
    return false;
  }
  g_state = "connected";
  LOGI("mqtt", "connected");
  publish(statusTopic(), "online", true);
  client.subscribe(cmdTopic().c_str());
  LOGI("mqtt", "subscribed %s", cmdTopic().c_str());
  for (const char *leaf : {"set", "position/set", "cmd"}) {
    String w = g_base + "/cover/+/" + leaf;
    client.subscribe(w.c_str());
    LOGI("mqtt", "subscribed %s", w.c_str());
  }
  for (const char *leaf : {"enable/set", "trip_lux/set", "clear_lux/set"}) {
    String w = solarTopic(leaf);
    client.subscribe(w.c_str());
    LOGI("mqtt", "subscribed %s", w.c_str());
  }
  publishDiscovery();            // hub diagnostic sensors
  publishShutterDiscovery();     // per-shutter cover + button entities
  publishSolarDiscovery();       // solar sensor/state/switch + threshold numbers
  publishState();
  resetStateCache();
  publishShutterStates(true);
  g_lastLux = -1; g_lastSolState = "";
  publishSolarStates(true);
  g_shDirty  = false;
  g_solDirty = false;
  g_nextStat = millis() + 30000;
  return true;
}
}  // namespace

namespace Mqtt {

void begin() {
  loadConfig();
  if (!g_enabled) { LOGI("mqtt", "disabled"); g_state = "disabled"; return; }
  LOGI("mqtt", "enabled — broker %s:%u, base '%s', HA discovery %s",
       g_host.c_str(), g_port, g_base.c_str(), g_haDisc ? "on" : "off");
  g_nextTry = 0;   // connect on the first loop()
}

void loop() {
  if (!g_enabled) return;
  if (WiFi.status() != WL_CONNECTED) return;

  if (!client.connected()) {
    if (millis() < g_nextTry) return;
    g_nextTry = millis() + 5000;         // retry every 5 s, non-blocking between tries
    tryConnect();
    return;
  }
  client.loop();
  if (g_shDirty) {                       // shutter config changed (web task) — refresh HA
    g_shDirty = false;
    LOGI("mqtt", "shutter config changed — refreshing HA discovery + state");
    publishShutterDiscovery();
    resetStateCache();
    publishShutterStates(true);
  }
  if (g_solDirty) {                      // solar config changed (web task) — echo it to HA
    g_solDirty = false;
    publishSolarStates(true);
  }
  if (millis() >= g_nextShPub) { g_nextShPub = millis() + SHUTTER_PUB_MS; publishShutterStates(false); }
  if (millis() >= g_nextSolPub) { g_nextSolPub = millis() + SOLAR_PUB_MS; publishSolarStates(false); }
  if (millis() >= g_nextStat)  { g_nextStat  = millis() + 30000; publishState(); }
}

void reconfigure() {
  LOGI("mqtt", "config changed — reloading");
  if (client.connected()) {
    publish(statusTopic(), "offline", true);
    client.disconnect();
  }
  loadConfig();
  g_state   = g_enabled ? "connecting" : "disabled";
  g_nextTry = 0;
}

void shuttersChanged() { g_shDirty = true; }
void solarChanged()    { g_solDirty = true; }

bool   connected() { return client.connected(); }
String stateText() { return g_state; }

}  // namespace Mqtt
