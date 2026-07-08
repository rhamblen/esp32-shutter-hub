// Shutters — per-blind definitions + calibration, persisted in NVS (Phase 2).
//
// Each shutter is one entry: a friendly name, a PCA9685 channel (declared now,
// not yet wired — see project-plan Phase 2), calibrated closed/open pulse widths,
// and two named favourites (Daylight / Privacy) per ADR-0005. Stored in its own
// Preferences namespace so a LittleFS OTA — or a config reset — never wipes hard-won
// calibration. Calibration motion drives the single ServoController servo for now.
#pragma once
#include <Arduino.h>

namespace Shutters {
const int MAX = 4;                 // one per physical shutter (4-panel target)
const int UNSET = -1;              // sentinel for an un-calibrated pulse width

void   begin();                    // load records from NVS
int    count();

String listJson();                 // JSON array of all shutters (id,name,channel,closedUs,openUs,daylightUs,privacyUs,calibrated)
bool   exists(const String &id);

// Index-based access for iteration (MQTT discovery + state publishing, Phase 4).
// Resolve an id to an index with find(); *At() getters return safe defaults when
// the index is out of range.
int    find(const String &id);     // index 0..count()-1, or -1 if unknown
String idAt(int i);
String nameAt(int i);
int    channelAt(int i);
bool   invertedAt(int i);
bool   calibratedAt(int i);        // both edges snapshotted

// Mutations — each persists immediately and returns success.
bool   add(const String &name, int channel);   // false if full; id auto-slugged from name
bool   remove(const String &id);
bool   rename(const String &id, const String &name);
bool   setChannel(const String &id, int channel);
bool   setInverted(const String &id, bool inverted);       // flip the position scale (0 % = open) for this blind
bool   setEdge(const String &id, bool openEdge, int us);   // snapshot closed/open; calibrated once both set
bool   saveFav(const String &id, bool privacy, int us);    // snapshot Daylight (false) / Privacy (true)
int    favUs(const String &id, bool privacy);              // stored pulse width, or UNSET
int    edgeUs(const String &id, bool openEdge);            // calibrated open/closed pulse width, or UNSET
}
