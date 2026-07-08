#include "Mqtt.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include <WiFi.h>
#include <PubSubClient.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

// Home Assistant MQTT discovery + hub availability. Every publish/receive is logged
// through the LOG* macros so it streams live to the Logs page.

namespace {
WiFiClient   net;
PubSubClient client(net);

bool          g_enabled   = false;
String        g_host, g_user, g_pass, g_clientId, g_base, g_node;
uint16_t      g_port      = 1883;
bool          g_haDisc    = true;
String        g_state     = "disabled";
unsigned long g_nextTry   = 0;         // next reconnect attempt (millis)
unsigned long g_nextStat  = 0;         // next periodic state publish (millis)

String statusTopic() { return g_base + "/status"; }
String stateTopic()  { return g_base + "/state"; }
String cmdTopic()    { return g_base + "/cmd"; }

// Sanitise a string into an MQTT/HA-safe id (alnum + underscore).
String slug(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    o += (isalnum((int)c) ? c : '_');
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

// One HA discovery sensor reading a field out of the JSON state topic.
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

void onMessage(char *topic, byte *payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len && i < 180; i++) msg += (char)payload[i];
  LOGI("mqtt", "rx %s: %s", topic, msg.c_str());
  // Command handling arrives with the shutter covers in Phase 4.
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
  publishDiscovery();
  publishState();
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
  if (millis() >= g_nextStat) { g_nextStat = millis() + 30000; publishState(); }
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

bool   connected() { return client.connected(); }
String stateText() { return g_state; }

}  // namespace Mqtt
