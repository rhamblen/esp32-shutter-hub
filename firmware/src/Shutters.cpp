#include "Shutters.h"
#include "Diagnostics.h"
#include <Preferences.h>

// Records live in memory and are re-serialised wholesale to NVS on every mutation
// (≤4 entries — cheap). The `id` slug is stored explicitly so a rename never
// changes the MQTT/HA identity of a shutter.
namespace {
Preferences prefs;
const char *NS = "shutters";       // own namespace — survives an app config reset

struct Shutter {
  String id;
  String name;
  int    channel   = 0;
  int    closedUs  = Shutters::UNSET;
  int    openUs    = Shutters::UNSET;
  int    daylightUs = Shutters::UNSET;
  int    privacyUs  = Shutters::UNSET;
  bool   inverted   = false;         // false: 0 % = closed (HA standard); true: 0 % = open
};

Shutter g_list[Shutters::MAX];
int     g_count = 0;

int indexOf(const String &id) {
  for (int i = 0; i < g_count; i++) if (g_list[i].id == id) return i;
  return -1;
}

String jesc(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o;
}

// name -> stable slug: lowercase, non-alnum -> '_', collapse/trim underscores,
// then de-duplicate against existing ids ("front_left", "front_left_2", …).
String slugify(const String &name) {
  String s;
  bool lastUnderscore = true;                 // suppress a leading underscore
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if (isalnum(c)) { s += (char)tolower(c); lastUnderscore = false; }
    else if (!lastUnderscore) { s += '_'; lastUnderscore = true; }
  }
  while (s.endsWith("_")) s.remove(s.length() - 1);
  if (!s.length()) s = "shutter";
  String base = s; int n = 2;
  while (indexOf(s) >= 0) s = base + "_" + String(n++);
  return s;
}

void persist() {
  prefs.putInt("count", g_count);
  for (int i = 0; i < g_count; i++) {
    const Shutter &s = g_list[i];
    prefs.putString(("id" + String(i)).c_str(), s.id);
    prefs.putString(("n"  + String(i)).c_str(), s.name);
    prefs.putInt(("c"  + String(i)).c_str(), s.channel);
    prefs.putInt(("cl" + String(i)).c_str(), s.closedUs);
    prefs.putInt(("op" + String(i)).c_str(), s.openUs);
    prefs.putInt(("dl" + String(i)).c_str(), s.daylightUs);
    prefs.putInt(("pv" + String(i)).c_str(), s.privacyUs);
    prefs.putBool(("in" + String(i)).c_str(), s.inverted);
  }
}

bool calibrated(const Shutter &s) {
  return s.closedUs != Shutters::UNSET && s.openUs != Shutters::UNSET;
}
}  // namespace

namespace Shutters {

void begin() {
  prefs.begin(NS, false);
  g_count = constrain(prefs.getInt("count", 0), 0, MAX);
  for (int i = 0; i < g_count; i++) {
    Shutter &s = g_list[i];
    s.id         = prefs.getString(("id" + String(i)).c_str(), "");
    s.name       = prefs.getString(("n"  + String(i)).c_str(), "");
    s.channel    = prefs.getInt(("c"  + String(i)).c_str(), i);
    s.closedUs   = prefs.getInt(("cl" + String(i)).c_str(), UNSET);
    s.openUs     = prefs.getInt(("op" + String(i)).c_str(), UNSET);
    s.daylightUs = prefs.getInt(("dl" + String(i)).c_str(), UNSET);
    s.privacyUs  = prefs.getInt(("pv" + String(i)).c_str(), UNSET);
    s.inverted   = prefs.getBool(("in" + String(i)).c_str(), false);
  }
  LOGI("shutters", "loaded %d shutter(s)", g_count);
}

int  count() { return g_count; }
bool exists(const String &id) { return indexOf(id) >= 0; }

String listJson() {
  String j = "[";
  for (int i = 0; i < g_count; i++) {
    const Shutter &s = g_list[i];
    if (i) j += ",";
    j += "{\"id\":\""   + jesc(s.id)   + "\"";
    j += ",\"name\":\"" + jesc(s.name) + "\"";
    j += ",\"channel\":"    + String(s.channel);
    j += ",\"closedUs\":"   + String(s.closedUs);
    j += ",\"openUs\":"     + String(s.openUs);
    j += ",\"daylightUs\":" + String(s.daylightUs);
    j += ",\"privacyUs\":"  + String(s.privacyUs);
    j += ",\"inverted\":"   + String(s.inverted ? "true" : "false");
    j += ",\"calibrated\":" + String(calibrated(s) ? "true" : "false");
    j += "}";
  }
  j += "]";
  return j;
}

bool add(const String &name, int channel) {
  if (g_count >= MAX) return false;
  Shutter &s = g_list[g_count];
  s = Shutter();                               // reset to defaults
  s.name    = name.length() ? name : String("Shutter ") + String(g_count + 1);
  s.id      = slugify(s.name);
  s.channel = constrain(channel, 0, 15);
  g_count++;
  persist();
  LOGI("shutters", "added '%s' (id=%s, ch=%d)", s.name.c_str(), s.id.c_str(), s.channel);
  return true;
}

bool remove(const String &id) {
  int i = indexOf(id);
  if (i < 0) return false;
  for (int k = i; k < g_count - 1; k++) g_list[k] = g_list[k + 1];
  g_count--;
  // Clear the now-orphaned tail slot's keys so a shrink can't resurrect stale data.
  int tail = g_count;
  for (const char *p : {"id", "n", "c", "cl", "op", "dl", "pv", "in"})
    prefs.remove((String(p) + String(tail)).c_str());
  persist();
  LOGI("shutters", "removed id=%s", id.c_str());
  return true;
}

bool rename(const String &id, const String &name) {
  int i = indexOf(id);
  if (i < 0 || !name.length()) return false;
  g_list[i].name = name;                       // id/slug is stable — not regenerated
  persist();
  return true;
}

bool setChannel(const String &id, int channel) {
  int i = indexOf(id);
  if (i < 0) return false;
  g_list[i].channel = constrain(channel, 0, 15);
  persist();
  return true;
}

bool setInverted(const String &id, bool inverted) {
  int i = indexOf(id);
  if (i < 0) return false;
  g_list[i].inverted = inverted;
  persist();
  LOGI("shutters", "%s scale %s", id.c_str(), inverted ? "inverted (0%%=open)" : "normal (0%%=closed)");
  return true;
}

bool setEdge(const String &id, bool openEdge, int us) {
  int i = indexOf(id);
  if (i < 0) return false;
  if (openEdge) g_list[i].openUs = us; else g_list[i].closedUs = us;
  persist();
  LOGI("shutters", "%s %s edge = %d µs", id.c_str(), openEdge ? "open" : "closed", us);
  return true;
}

bool saveFav(const String &id, bool privacy, int us) {
  int i = indexOf(id);
  if (i < 0) return false;
  if (privacy) g_list[i].privacyUs = us; else g_list[i].daylightUs = us;
  persist();
  LOGI("shutters", "%s %s = %d µs", id.c_str(), privacy ? "privacy" : "daylight", us);
  return true;
}

int favUs(const String &id, bool privacy) {
  int i = indexOf(id);
  if (i < 0) return UNSET;
  return privacy ? g_list[i].privacyUs : g_list[i].daylightUs;
}

int edgeUs(const String &id, bool openEdge) {
  int i = indexOf(id);
  if (i < 0) return UNSET;
  return openEdge ? g_list[i].openUs : g_list[i].closedUs;
}

}  // namespace Shutters
