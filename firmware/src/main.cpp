// ESP32 Smart Shutter Hub — firmware framework (v0.0.1)
// -----------------------------------------------------------------------------
// Minimal base image: on-device WiFi setup, mDNS, a status page, and OTA.
// Everything else (ServoController, HomeKit, Mqtt, LightSensor, Config, ...) bolts
// onto this later — see docs/project-plan.md and the module list in ADR 0004.
//
// WiFi provisioning
//   First boot (or after "Change WiFi") the hub becomes an access point called
//   "Shutter-Hub-Setup" with a captive portal — join it from a phone/laptop and
//   pick your network. Credentials are stored in NVS, a *separate* flash partition
//   from the application, so they SURVIVE over-the-air firmware updates.
//   This mirrors rednblkx/HomeKey-ESP32 (SoftAP + captive portal), implemented on
//   Arduino Core with WiFiManager per the project brief — no credentials in the
//   binary, so the same bin works on anyone's network.
//
// Runs on a bare ESP32 dev board — no PCA9685, servos or power hardware needed yet.
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>          // tzapu/WiFiManager — AP + captive portal, creds in NVS
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.1"
#endif

static const char *HOSTNAME = "shutter-hub";            // -> http://shutter-hub.local
static const char *AP_NAME  = "Shutter-Hub-Setup";      // SSID shown when unprovisioned
static const uint16_t PORTAL_TIMEOUT_S = 180;           // reboot & retry if left unconfigured

static AsyncWebServer server(80);
static unsigned long  bootMillis = 0;
static bool           forgetWiFiPending = false;

// ---- Status page ------------------------------------------------------------
// Embedded in the firmware for now (single-binary OTA). Migrates to a LittleFS
// data/ folder in Phase 2 when the real calibration UI arrives.

static String humanUptime() {
  unsigned long s = (millis() - bootMillis) / 1000UL;
  unsigned long d = s / 86400UL; s %= 86400UL;
  unsigned long h = s / 3600UL;  s %= 3600UL;
  unsigned long m = s / 60UL;    s %= 60UL;
  char buf[48];
  snprintf(buf, sizeof(buf), "%lud %02luh %02lum %02lus", d, h, m, s);
  return String(buf);
}

static String statusPage() {
  String html = F(
    "<!doctype html><html lang=en><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Shutter Hub</title><style>"
    ":root{color-scheme:light dark}"
    "body{font-family:system-ui,sans-serif;max-width:34rem;margin:2rem auto;padding:0 1rem;line-height:1.5}"
    "h1{font-size:1.4rem;margin-bottom:.25rem}.sub{opacity:.6;margin-top:0}"
    "table{border-collapse:collapse;width:100%;margin:1rem 0}"
    "td{padding:.35rem .5rem;border-bottom:1px solid #8884}td:first-child{opacity:.65;white-space:nowrap}"
    "td:last-child{text-align:right;font-variant-numeric:tabular-nums}"
    ".btn{display:inline-block;padding:.6rem 1rem;border:1px solid #8886;border-radius:.5rem;"
    "text-decoration:none;background:none;color:inherit;font:inherit;cursor:pointer}"
    ".row{display:flex;gap:.5rem;flex-wrap:wrap}"
    "</style></head><body>"
    "<h1>ESP32 Shutter Hub</h1><p class=sub>Framework scaffold &middot; firmware v" FW_VERSION "</p>"
    "<table>");
  html += "<tr><td>WiFi network</td><td>" + WiFi.SSID() + "</td></tr>";
  html += "<tr><td>Hostname</td><td>" + String(HOSTNAME) + ".local</td></tr>";
  html += "<tr><td>IP address</td><td>" + WiFi.localIP().toString() + "</td></tr>";
  html += "<tr><td>MAC</td><td>" + WiFi.macAddress() + "</td></tr>";
  html += "<tr><td>WiFi RSSI</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  html += "<tr><td>Uptime</td><td>" + humanUptime() + "</td></tr>";
  html += "<tr><td>Free heap</td><td>" + String(ESP.getFreeHeap() / 1024) + " KB</td></tr>";
  html += F("</table><div class=row>"
    "<a class=btn href='/update'>Firmware update (OTA) &rarr;</a>"
    "<form method=POST action='/forget-wifi' "
    "onsubmit=\"return confirm('Clear saved WiFi and restart into setup mode?')\">"
    "<button class=btn type=submit>Change WiFi&hellip;</button></form>"
    "</div></body></html>");
  return html;
}

// ---- WiFi (WiFiManager: AP + captive portal, creds persisted in NVS) --------

static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);

  WiFiManager wm;
  wm.setHostname(HOSTNAME);
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);  // don't sit in AP forever if unattended
  wm.setAPCallback([](WiFiManager *m) {
    Serial.printf("[wifi] no saved network — starting setup AP \"%s\"\n", AP_NAME);
    Serial.println("[wifi] join it and browse to http://192.168.4.1 to choose your WiFi");
  });

  // Tries stored creds (kept in NVS from last time) first; only opens the portal
  // if that fails. Returns false on portal timeout -> reboot and retry.
  if (!wm.autoConnect(AP_NAME)) {
    Serial.println("[wifi] not configured / timed out — rebooting to retry");
    delay(3000);
    ESP.restart();
  }
  Serial.printf("[wifi] connected to \"%s\": %s (RSSI %d dBm)\n",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

// ---- OTA callbacks (serial breadcrumbs) -------------------------------------

static void onOtaStart()            { Serial.println("[ota] update started"); }
static void onOtaProgress(size_t c, size_t t) {
  static unsigned long last = 0;
  if (millis() - last > 1000UL) { last = millis();
    Serial.printf("[ota] %u / %u bytes\n", (unsigned)c, (unsigned)t); }
}
static void onOtaEnd(bool ok)       { Serial.printf("[ota] %s\n", ok ? "success — rebooting" : "FAILED"); }

// ---- Arduino entry points ---------------------------------------------------

void setup() {
  bootMillis = millis();
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== ESP32 Shutter Hub — firmware v%s ===\n", FW_VERSION);

  connectWiFi();

  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[mdns] http://%s.local\n", HOSTNAME);
  } else {
    Serial.println("[mdns] start failed");
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", statusPage());
  });
  server.on("/healthz", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/plain", "ok");
  });
  // Clear saved WiFi and reboot into the setup AP. Reset is deferred to loop()
  // so the HTTP response flushes first.
  server.on("/forget-wifi", HTTP_POST, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html",
      "<meta http-equiv=refresh content='10;url=/'>"
      "<p style='font-family:system-ui'>WiFi cleared. The hub is restarting as "
      "<b>Shutter-Hub-Setup</b> — join that network to choose a new WiFi.</p>");
    forgetWiFiPending = true;
  });

  ElegantOTA.onStart(onOtaStart);
  ElegantOTA.onProgress(onOtaProgress);
  ElegantOTA.onEnd(onOtaEnd);
  ElegantOTA.begin(&server);        // browser OTA at /update

  server.begin();
  Serial.println("[http] server up — open the IP above or shutter-hub.local");
}

void loop() {
  ElegantOTA.loop();

  if (forgetWiFiPending) {
    delay(400);                     // let the response flush
    Serial.println("[wifi] clearing saved credentials and restarting");
    WiFiManager wm;
    wm.resetSettings();             // wipes stored SSID/pass from NVS
    delay(200);
    ESP.restart();
  }

  delay(5);
}
