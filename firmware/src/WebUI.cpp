#include "WebUI.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include "WiFiSetup.h"
#include "Ota.h"
#include "Mqtt.h"
#include "ServoController.h"
#include "Shutters.h"
#include "HomeKit.h"
#include "LightSensor.h"
#include "SolarLogic.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <time.h>

// WebUI (v0.2.0) — the sidebar single-page app is static files in LittleFS
// (data/index.html, style.css, app.js). The firmware serves those plus a JSON/REST
// API and a WebSocket log feed. If the filesystem image hasn't been flashed yet a
// tiny embedded bootstrap page still offers OTA, so a device can't strand itself.

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif
#ifndef FW_VARIANT
#define FW_VARIANT "unknown"
#endif

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws/logs");
static bool   pendingWifiConnect = false;   // in-place network switch (not a reboot)
static String connSsid, connPass;

// escape a string for embedding in JSON
static String jesc(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o;
}

// Web-auth gate: true if the request may proceed; otherwise a 401 is sent.
static bool guard(AsyncWebServerRequest *r) {
  if (!AppConfig::authEnabled()) return true;
  if (!r->authenticate(AppConfig::authUser().c_str(), AppConfig::authPass().c_str())) {
    r->requestAuthentication();
    return false;
  }
  return true;
}

// ---- JSON builders ----------------------------------------------------------

static String dashInfoJson() {
  String j = "{";
  j += "\"fw\":\"" FW_VERSION "\",";
  j += "\"variant\":\"" FW_VARIANT "\",";
  j += "\"backend\":\"" + String(ServoController::backend()) + "\",";
  j += "\"chip\":\"" + String(ESP.getChipModel()) + "\",";
  j += "\"device\":\"" + jesc(AppConfig::deviceName()) + "\",";
  j += "\"host\":\"" + jesc(AppConfig::deviceName()) + ".local\",";
  j += "\"uptime\":\"" + Diagnostics::humanUptime() + "\",";
  j += "\"boot_count\":" + String(AppConfig::bootCount()) + ",";
  j += "\"reset_reason\":\"" + Diagnostics::resetReason() + "\",";
  j += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  j += "\"min_free_heap\":" + String(ESP.getMinFreeHeap()) + ",";
  j += "\"wifi\":{";
  j += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  j += "\"ssid\":\"" + jesc(WiFi.SSID()) + "\",";
  j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  j += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  j += "\"mac\":\"" + WiFi.macAddress() + "\"},";
  j += "\"mqtt\":{\"enabled\":" + String(AppConfig::mqttEnabled() ? "true" : "false") +
       ",\"connected\":" + String(Mqtt::connected() ? "true" : "false") +
       ",\"state\":\"" + jesc(Mqtt::stateText()) + "\"},";
  j += "\"last_flash\":{\"type\":\"" + jesc(AppConfig::lastFlashType()) +
       "\",\"ok\":" + String(AppConfig::lastFlashOk() ? "true" : "false") +
       ",\"epoch\":" + String(AppConfig::lastFlashEpoch()) + "},";
  // Hardware map (v0.6.2) — enough to check the wiring against the board without
  // opening three other pages: which device on which bus, on which pins, at which
  // address/channel. `pin` is meaningless on a PCA9685 build (ServoController::pin()
  // aliases the channel there), so the UI keys everything off `usesPca`.
  j += "\"servo\":{\"usesPca\":" + String(ServoController::usesPca() ? "true" : "false") +
       ",\"pin\":"     + String(ServoController::pin()) +
       ",\"channel\":" + String(ServoController::channel()) +
       ",\"sda\":"     + String(ServoController::sdaPin()) +
       ",\"scl\":"     + String(ServoController::sclPin()) +
       ",\"attached\":" + String(ServoController::attached() ? "true" : "false") + "},";
  j += "\"sensor\":{\"enabled\":" + String(LightSensor::enabled() ? "true" : "false") +
       ",\"present\":" + String(LightSensor::present() ? "true" : "false") +
       ",\"bus\":"     + String(LightSensor::activeBus()) +
       ",\"sda\":"     + String(LightSensor::activeSda()) +
       ",\"scl\":"     + String(LightSensor::activeScl()) +
       ",\"dedicatedSupported\":" + String(LightSensor::dedicatedSupported() ? "true" : "false") + "},";
  j += "\"homekit\":{\"enabled\":" + String(AppConfig::hkEnabled() ? "true" : "false") +
       ",\"running\":" + String(HomeKit::running() ? "true" : "false") +
       ",\"paired\":"  + String(HomeKit::paired() ? "true" : "false") +
       ",\"controllers\":" + String(HomeKit::controllers()) + "},";
  j += "\"shutters\":[";
  for (int i = 0; i < Shutters::count(); i++) {
    if (i) j += ",";
    j += "{\"name\":\"" + jesc(Shutters::nameAt(i)) + "\",\"channel\":" + String(Shutters::channelAt(i)) +
         ",\"calibrated\":" + String(Shutters::calibratedAt(i) ? "true" : "false") + "}";
  }
  j += "]";
  j += "}";
  return j;
}

static String mqttConfigJson() {
  String j = "{";
  j += "\"enabled\":" + String(AppConfig::mqttEnabled() ? "true" : "false") + ",";
  j += "\"host\":\"" + jesc(AppConfig::mqttHost()) + "\",";
  j += "\"port\":" + String(AppConfig::mqttPort()) + ",";
  j += "\"clientId\":\"" + jesc(AppConfig::mqttClientId()) + "\",";
  j += "\"user\":\"" + jesc(AppConfig::mqttUser()) + "\",";
  j += "\"hasPass\":" + String(AppConfig::mqttPass().length() ? "true" : "false") + ",";
  j += "\"base\":\"" + jesc(AppConfig::mqttBaseTopic()) + "\",";
  j += "\"haDiscovery\":" + String(AppConfig::mqttHaDiscovery() ? "true" : "false") + ",";
  j += "\"connected\":" + String(Mqtt::connected() ? "true" : "false") + ",";
  j += "\"state\":\"" + jesc(Mqtt::stateText()) + "\"";
  j += "}";
  return j;
}

static String hkConfigJson() {
  String j = "{";
  j += "\"enabled\":" + String(AppConfig::hkEnabled() ? "true" : "false") + ",";
  j += "\"name\":\"" + jesc(AppConfig::hkBridgeName()) + "\",";
  j += "\"code\":\"" + AppConfig::hkSetupCode() + "\",";
  j += "\"running\":" + String(HomeKit::running() ? "true" : "false") + ",";
  j += "\"paired\":" + String(HomeKit::paired() ? "true" : "false") + ",";
  j += "\"controllers\":" + String(HomeKit::controllers());
  j += "}";
  return j;
}

static String solarConfigJson() {
  String j = "{";
  j += "\"enabled\":" + String(AppConfig::solarEnabled() ? "true" : "false") + ",";
  j += "\"sensor\":{";
  j +=   "\"enabled\":" + String(AppConfig::lsEnabled() ? "true" : "false") + ",";
  j +=   "\"type\":"    + String(AppConfig::lsType()) + ",";
  j +=   "\"bus\":"     + String(AppConfig::lsBus()) + ",";        // preference (0 dedicated, 1 shared)
  j +=   "\"sda\":"     + String(AppConfig::lsSda()) + ",";        // dedicated-bus pins (form fields)
  j +=   "\"scl\":"     + String(AppConfig::lsScl()) + ",";
  j +=   "\"dedicatedSupported\":" + String(LightSensor::dedicatedSupported() ? "true" : "false") + ",";
  j +=   "\"activeBus\":" + String(LightSensor::activeBus()) + ",";  // after clamping on a 1-bus SoC
  j +=   "\"activeSda\":" + String(LightSensor::activeSda()) + ",";
  j +=   "\"activeScl\":" + String(LightSensor::activeScl()) + ",";
  j +=   "\"present\":" + String(LightSensor::present() ? "true" : "false") + ",";
  j +=   "\"lux\":"     + String(LightSensor::lux(), 0) + ",";
  j +=   "\"brightness\":" + String(LightSensor::brightnessPct()) + ",";   // 0-100 %, log scale
  j +=   "\"simulated\":" + String(LightSensor::simulated() ? "true" : "false");
  j += "},";
  j += "\"trip\":{\"lux\":"  + String(AppConfig::solarTripLux())  + ",\"secs\":" + String(AppConfig::solarTripSecs())  + "},";
  j += "\"clear\":{\"lux\":" + String(AppConfig::solarClearLux()) + ",\"secs\":" + String(AppConfig::solarClearSecs()) + "},";
  j += "\"brightTarget\":" + String(AppConfig::solarBrightTarget()) + ",";
  j += "\"clearTarget\":"  + String(AppConfig::solarClearTarget()) + ",";
  j += "\"state\":\""    + String(SolarLogic::stateText()) + "\",";
  j += "\"remaining\":"  + String(SolarLogic::secondsRemaining()) + ",";
  j += "\"suspended\":"  + String(SolarLogic::suspendedCount());
  j += "}";
  return j;
}

// Apple rejects trivial pairing codes; enforce the same list so a code that HomeSpan
// would refuse can never be saved. 8 digits, not all-same, not 12345678/87654321.
static bool validSetupCode(const String &c) {
  if (c.length() != 8) return false;
  bool allSame = true;
  for (size_t i = 0; i < 8; i++) {
    if (c[i] < '0' || c[i] > '9') return false;
    if (c[i] != c[0]) allSame = false;
  }
  return !allSame && c != "12345678" && c != "87654321";
}

// ---- Embedded fallback (only used when data/ isn't flashed) ------------------

static String bootstrapPage() {
  String h = F("<!doctype html><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Shutter Hub — recovery</title>"
    "<body style='font-family:system-ui;max-width:34rem;margin:2rem auto;padding:0 1rem'>"
    "<h1>Shutter Hub</h1>"
    "<p style='color:#c0392b'><b>Web UI filesystem not found.</b> Upload the LittleFS image "
    "(and/or firmware) below, then reboot. Firmware v" FW_VERSION ".</p>"
    "<p><label>Firmware (.bin) <input type=file id=fw accept='.bin'></label></p>"
    "<p><label>Filesystem (.bin) <input type=file id=fs accept='.bin'></label></p>"
    "<button id=go>Upload</button> <span id=s></span>"
    "<script>"
    "function up(t,f){return new Promise(function(res,rej){var x=new XMLHttpRequest();"
    "x.open('POST','/api/ota?target='+t);x.onload=function(){(x.status==200)?res():rej(x.responseText);};"
    "x.onerror=function(){rej('network');};var d=new FormData();d.append('file',f);x.send(d);});}"
    "document.getElementById('go').onclick=function(){var s=document.getElementById('s');"
    "var fw=document.getElementById('fw').files[0],fs=document.getElementById('fs').files[0];"
    "var p=Promise.resolve();if(fs)p=p.then(function(){s.textContent='fs\\u2026';return up('filesystem',fs);});"
    "if(fw)p=p.then(function(){s.textContent='fw\\u2026';return up('firmware',fw);});"
    "p.then(function(){s.textContent='done — reboot';}).catch(function(e){s.textContent='failed: '+e;});};"
    "</script>");
  return h;
}

// ---- WebSocket log feed -----------------------------------------------------

static void broadcastLog(const String &jsonLine) {
  if (ws.count() == 0) return;
  ws.textAll(jsonLine);
}

static void onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type,
                      void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    c->text(Diagnostics::logHistoryJson());   // catch the client up on buffered lines
  }
}

// ---- Public API -------------------------------------------------------------

namespace WebUI {

void begin() {
  String host = AppConfig::deviceName();

  // mDNS ownership: HomeSpan and Arduino ESPmDNS share ONE underlying responder. We initialise it
  // HERE, on the main thread, for BOTH modes. Earlier this was DEFERRED to HomeSpan whenever HomeKit
  // was enabled — but HomeSpan calls mdns_init() from its background autoPoll task (core 0, alongside
  // WiFiManager-owned WiFi + AsyncTCP), and that call HANGS on this build: the responder never came
  // up (no hostname, no _hap/_http) and the HAP server on port 1201 never started, so the Home app
  // could never discover *or* pair with the bridge (v0.7.0 diagnosis: onConnection never fired, 1201
  // closed, `dns-sd` saw nothing for shutter-hub). Bringing mDNS up on the main thread is the path
  // that always worked (it's what ran with HomeKit off). HomeSpan's checkConnect() then calls
  // mdns_init() too, but with the responder already running that returns ESP_ERR_INVALID_STATE
  // immediately (a cheap no-op, not a hang) and HomeSpan simply ADDS its _hap._tcp service on top.
  if (MDNS.begin(host.c_str())) {
    MDNS.addService("http", "tcp", 80);
    LOGI("mdns", "http://%s.local%s", host.c_str(),
         AppConfig::hkEnabled() ? " (HomeSpan will add _hap._tcp)" : "");
  } else {
    LOGW("mdns", "start failed");
  }

  // Live log feed: register the sink so every LOG* line streams to WS clients.
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  Diagnostics::setLogSink(broadcastLog);

  // ---- Read-only info ----
  server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", dashInfoJson());
  });
  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *r) {   // legacy raw diagnostics
    r->send(200, "application/json", Diagnostics::infoJson());
  });
  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", Diagnostics::logHistoryJson());
  });
  server.on("/healthz", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", "ok");
  });

  // ---- MQTT config ----
  server.on("/api/mqtt", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", mqttConfigJson());
  });
  server.on("/api/mqtt", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    auto p = [&](const char *k, const String &d) {
      return r->hasParam(k, true) ? r->getParam(k, true)->value() : d;
    };
    bool     en   = p("enabled", "false") == "true" || p("enabled", "") == "1";
    String   host = p("host", AppConfig::mqttHost());
    uint16_t port = (uint16_t)p("port", String(AppConfig::mqttPort())).toInt();
    String   cid  = p("clientId", "");
    String   user = p("user", AppConfig::mqttUser());
    // Blank password field => keep the stored password (we never echo it back).
    String   pass = r->hasParam("pass", true) && r->getParam("pass", true)->value().length()
                      ? r->getParam("pass", true)->value() : AppConfig::mqttPass();
    String   base = p("base", AppConfig::mqttBaseTopic());
    bool     ha   = p("haDiscovery", "true") == "true" || p("haDiscovery", "") == "1";
    AppConfig::setMqtt(en, host, port, cid, user, pass, base, ha);
    Mqtt::reconfigure();
    r->send(200, "application/json", mqttConfigJson());
  });

  // ---- System quick actions ----
  // Each sends its response first, then schedules the restart via a high-priority
  // esp_timer (Diagnostics::scheduleReboot) so it fires even if the main loop is stalled.
  server.on("/api/system/reboot", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", "{\"ok\":true,\"msg\":\"rebooting\"}");
    Diagnostics::scheduleReboot(600);
  });
  server.on("/api/system/reset-wifi", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json",
      "{\"ok\":true,\"msg\":\"WiFi cleared — restarting into Shutter-Hub-Setup\"}");
    WiFiSetup::forget();                 // clear creds now; the timer restarts us shortly
    Diagnostics::scheduleReboot(600);
  });
  server.on("/api/system/reset-config", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json",
      "{\"ok\":true,\"msg\":\"settings cleared — rebooting (WiFi kept)\"}");
    AppConfig::factoryReset();           // wipe app settings now; timer restarts us shortly
    Diagnostics::scheduleReboot(600);
  });

  // ---- Security (web auth) ----
  server.on("/api/auth", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json",
      String("{\"enabled\":") + (AppConfig::authEnabled() ? "true" : "false") +
      ",\"user\":\"" + jesc(AppConfig::authUser()) + "\"}");
  });
  server.on("/api/auth", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    bool   en   = r->hasParam("enabled", true) &&
                  (r->getParam("enabled", true)->value() == "true" ||
                   r->getParam("enabled", true)->value() == "1");
    String user = r->hasParam("user", true) ? r->getParam("user", true)->value() : AppConfig::authUser();
    String pass = r->hasParam("pass", true) && r->getParam("pass", true)->value().length()
                    ? r->getParam("pass", true)->value() : AppConfig::authPass();
    AppConfig::setAuth(en, user, pass);
    r->send(200, "application/json", "{\"ok\":true}");
  });

  // ---- HomeKit (System > HomeKit sub-tab — config + HomeSpan bridge state, Phase 5) ----
  server.on("/api/homekit", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", hkConfigJson());
  });
  server.on("/api/homekit", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    bool   en   = r->hasParam("enabled", true) &&
                  (r->getParam("enabled", true)->value() == "true" ||
                   r->getParam("enabled", true)->value() == "1");
    String name = r->hasParam("name", true) ? r->getParam("name", true)->value() : AppConfig::hkBridgeName();
    String code = r->hasParam("code", true) ? r->getParam("code", true)->value() : AppConfig::hkSetupCode();
    if (!validSetupCode(code)) {
      r->send(400, "application/json",
        "{\"error\":\"setup code must be 8 digits and not trivial (all-same, 12345678, 87654321)\"}");
      return;
    }
    AppConfig::setHomeKit(en, name, code);
    LOGI("homekit", "config saved: enabled=%d name='%s' — REBOOT to apply (bridge starts at boot)",
         en, AppConfig::hkBridgeName().c_str());
    r->send(200, "application/json", hkConfigJson());
  });
  server.on("/api/homekit/reset-pairings", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!HomeKit::resetPairings()) {
      r->send(409, "application/json",
        "{\"error\":\"HomeKit bridge isn't running — enable it and reboot first\"}");
      return;
    }
    r->send(200, "application/json", "{\"ok\":true,\"msg\":\"pairings cleared — re-pair from the Home app\"}");
  });

  // ---- Solar (light sensor + heat protection, Phase 6) ----
  server.on("/api/solar", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", solarConfigJson());
  });
  server.on("/api/solar", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    auto p = [&](const char *k, const String &d) {
      return r->hasParam(k, true) ? r->getParam(k, true)->value() : d;
    };
    auto b = [&](const char *k) { return p(k, "") == "true" || p(k, "") == "1"; };
    // Sensor (bus choice + pins + type + enable). Minutes in the form → seconds in NVS.
    bool     lsEn    = b("lsEnabled");
    uint8_t  lsType  = (uint8_t) p("lsType", String(AppConfig::lsType())).toInt();
    uint8_t  lsBus   = (uint8_t) p("bus", String(AppConfig::lsBus())).toInt();
    uint8_t  sda     = (uint8_t) p("sda", String(AppConfig::lsSda())).toInt();
    uint8_t  scl     = (uint8_t) p("scl", String(AppConfig::lsScl())).toInt();
    if (lsBus > AppConfig::BUS_SHARED) lsBus = AppConfig::BUS_DEDICATED;
    if (lsBus == AppConfig::BUS_DEDICATED) {
      if (!LightSensor::dedicatedSupported()) {
        r->send(400, "application/json",
          "{\"error\":\"this chip has only one I2C controller — use the shared bus\"}");
        return;
      }
      // Dedicated pins are only ever used on a two-controller SoC (the ESP32-D), so the
      // servo GPIO whitelist is the right validator here — it rejects 34–39 and the strapping pins.
      if (sda == scl || !ServoController::isValidPin(sda) || !ServoController::isValidPin(scl)) {
        r->send(400, "application/json",
          "{\"error\":\"invalid dedicated-bus pins (must differ, be output-capable, not GPIO34-39)\"}");
        return;
      }
    }
    // Solar thresholds + targets.
    bool     solEn   = b("enabled");
    uint32_t tripLux = (uint32_t) p("tripLux",  String(AppConfig::solarTripLux())).toInt();
    uint16_t tripS   = (uint16_t)(p("tripMin",  String(AppConfig::solarTripSecs()  / 60)).toInt() * 60);
    uint32_t clrLux  = (uint32_t) p("clearLux", String(AppConfig::solarClearLux())).toInt();
    uint16_t clrS    = (uint16_t)(p("clearMin", String(AppConfig::solarClearSecs() / 60)).toInt() * 60);
    uint8_t  bright  = (uint8_t) p("brightTarget", String(AppConfig::solarBrightTarget())).toInt();
    uint8_t  clear   = (uint8_t) p("clearTarget",  String(AppConfig::solarClearTarget())).toInt();
    AppConfig::setLightSensor(lsEn, lsType, lsBus, sda, scl);
    AppConfig::setSolar(solEn, tripLux, tripS, clrLux, clrS, bright, clear);
    LightSensor::reconfigure();          // apply bus choice / pins / enable without a reboot
    Mqtt::solarChanged();                // echo the new thresholds/switch state to HA
    LOGI("solar", "config saved: sensor=%d on the %s bus (SDA%u/SCL%u), automation=%d",
         lsEn, LightSensor::activeBus() == AppConfig::BUS_DEDICATED ? "dedicated" : "shared",
         LightSensor::activeSda(), LightSensor::activeScl(), solEn);
    r->send(200, "application/json", solarConfigJson());
  });
  // Force an effective lux for testing (?lux=N) or drop back to the live sensor (?live=1).
  server.on("/api/solar/simulate", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (r->hasParam("live", true))     LightSensor::useLive();
    else if (r->hasParam("lux", true)) LightSensor::simulate(r->getParam("lux", true)->value().toFloat());
    r->send(200, "application/json", solarConfigJson());
  });

  // ---- WiFi (feeds the System > WiFi sub-tab) ----
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) { r->send(202, "application/json", "{\"scanning\":true}"); return; }
    if (n == WIFI_SCAN_FAILED)  { WiFi.scanNetworks(true); r->send(202, "application/json", "{\"scanning\":true}"); return; }
    String j = "[";
    for (int i = 0; i < n; i++) {
      if (i) j += ",";
      j += "{\"ssid\":\"" + jesc(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
           ",\"lock\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
    }
    j += "]";
    WiFi.scanDelete();
    r->send(200, "application/json", j);
  });
  server.on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("ssid", true)) { r->send(400, "application/json", "{\"error\":\"missing ssid\"}"); return; }
    connSsid = r->getParam("ssid", true)->value();
    connPass = r->hasParam("pass", true) ? r->getParam("pass", true)->value() : "";
    pendingWifiConnect = true;
    r->send(200, "application/json",
      "{\"ok\":true,\"msg\":\"Connecting to '" + jesc(connSsid) + "' — if the password is right the hub "
      "moves to that network (reconnect at " + AppConfig::deviceName() + ".local). A wrong password reverts.\"}");
  });

  // ---- Servo / Actions (Phase 1) ----
  server.on("/api/servo", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", ServoController::statusJson());
  });
  server.on("/api/servo/pin", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("gpio")) { r->send(400, "application/json", "{\"error\":\"missing gpio\"}"); return; }
    long g = r->getParam("gpio")->value().toInt();
    if (g < 0 || g > 39 || !ServoController::setPin((uint8_t)g)) {
      r->send(400, "application/json", "{\"error\":\"GPIO not usable for servo output\"}"); return;
    }
    r->send(200, "application/json", ServoController::statusJson());
  });
  // PCA9685 build only — pick which channel the bench test drives (0–15).
  server.on("/api/servo/channel", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("ch")) { r->send(400, "application/json", "{\"error\":\"missing ch\"}"); return; }
    long ch = r->getParam("ch")->value().toInt();
    if (ch < 0 || ch > 15 || !ServoController::setChannel((uint8_t)ch)) {
      r->send(400, "application/json", "{\"error\":\"channel out of range (0-15) or not a PCA9685 build\"}"); return;
    }
    r->send(200, "application/json", ServoController::statusJson());
  });
  // PCA9685 build only — set the I2C bus pins the PCA9685 sits on (SDA/SCL).
  server.on("/api/servo/i2c", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("sda") || !r->hasParam("scl")) {
      r->send(400, "application/json", "{\"error\":\"missing sda/scl\"}"); return; }
    long sda = r->getParam("sda")->value().toInt();
    long scl = r->getParam("scl")->value().toInt();
    if (sda < 0 || sda > 39 || scl < 0 || scl > 39 ||
        !ServoController::setI2cPins((uint8_t)sda, (uint8_t)scl)) {
      r->send(400, "application/json", "{\"error\":\"I2C pins invalid or not a PCA9685 build\"}"); return;
    }
    r->send(200, "application/json", ServoController::statusJson());
  });
  server.on("/api/servo/write", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("deg")) { r->send(400, "application/json", "{\"error\":\"missing deg\"}"); return; }
    ServoController::writeAngle((int)r->getParam("deg")->value().toInt());
    r->send(200, "application/json", ServoController::statusJson());
  });
  server.on("/api/servo/speed", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("dps")) { r->send(400, "application/json", "{\"error\":\"missing dps\"}"); return; }
    long d = r->getParam("dps")->value().toInt();
    if (d < 0 || d > 1000) { r->send(400, "application/json", "{\"error\":\"dps out of range (0-1000)\"}"); return; }
    ServoController::setSpeed((uint16_t)d);
    r->send(200, "application/json", ServoController::statusJson());
  });
  server.on("/api/servo/attach", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    ServoController::attach();
    r->send(200, "application/json", ServoController::statusJson());
  });
  server.on("/api/servo/detach", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    ServoController::detach();
    r->send(200, "application/json", ServoController::statusJson());
  });
  server.on("/api/servo/sweep", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (ServoController::sweeping()) ServoController::stopSweep(); else ServoController::startSweep();
    r->send(200, "application/json", ServoController::statusJson());
  });

  // Microsecond control for blind calibration (Shutters page). Absolute move,
  // frame-step nudge, and speed-limited slow-run in a direction.
  server.on("/api/servo/us", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("us")) { r->send(400, "application/json", "{\"error\":\"missing us\"}"); return; }
    ServoController::writeUs((int)r->getParam("us")->value().toInt());
    r->send(200, "application/json", ServoController::statusJson());
  });
  server.on("/api/servo/jog", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("delta")) { r->send(400, "application/json", "{\"error\":\"missing delta\"}"); return; }
    ServoController::jogUs((int)r->getParam("delta")->value().toInt());
    r->send(200, "application/json", ServoController::statusJson());
  });
  server.on("/api/servo/run", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    String d = r->hasParam("dir") ? r->getParam("dir")->value() : String("stop");
    ServoController::run(d == "open" ? 1 : d == "close" ? -1 : 0);
    r->send(200, "application/json", ServoController::statusJson());
  });

  // ---- Shutters (Phase-2 config + calibration) -----------------------------
  server.on("/api/shutters", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    r->send(200, "application/json", Shutters::listJson());
  });
  server.on("/api/shutters/add", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    String name = r->hasParam("name", true) ? r->getParam("name", true)->value() : String("");
    int ch = r->hasParam("channel", true) ? r->getParam("channel", true)->value().toInt() : Shutters::count();
    if (!Shutters::add(name, ch)) { r->send(400, "application/json", "{\"error\":\"maximum shutters reached\"}"); return; }
    Mqtt::shuttersChanged();
    r->send(200, "application/json", Shutters::listJson());
  });
  server.on("/api/shutters/remove", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("id", true) || !Shutters::remove(r->getParam("id", true)->value())) {
      r->send(400, "application/json", "{\"error\":\"unknown shutter\"}"); return; }
    Mqtt::shuttersChanged();
    r->send(200, "application/json", Shutters::listJson());
  });
  server.on("/api/shutters/rename", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("id", true) || !r->hasParam("name", true) ||
        !Shutters::rename(r->getParam("id", true)->value(), r->getParam("name", true)->value())) {
      r->send(400, "application/json", "{\"error\":\"rename failed\"}"); return; }
    Mqtt::shuttersChanged();
    r->send(200, "application/json", Shutters::listJson());
  });
  server.on("/api/shutters/channel", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("id", true) || !r->hasParam("channel", true) ||
        !Shutters::setChannel(r->getParam("id", true)->value(), r->getParam("channel", true)->value().toInt())) {
      r->send(400, "application/json", "{\"error\":\"unknown shutter\"}"); return; }
    Mqtt::shuttersChanged();
    r->send(200, "application/json", Shutters::listJson());
  });
  server.on("/api/shutters/invert", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("id", true) || !r->hasParam("inverted", true)) {
      r->send(400, "application/json", "{\"error\":\"missing id/inverted\"}"); return; }
    bool inv = r->getParam("inverted", true)->value() == "true" || r->getParam("inverted", true)->value() == "1";
    if (!Shutters::setInverted(r->getParam("id", true)->value(), inv)) {
      r->send(400, "application/json", "{\"error\":\"unknown shutter\"}"); return; }
    Mqtt::shuttersChanged();
    r->send(200, "application/json", Shutters::listJson());
  });
  // Snapshot the servo's current pulse width into an endpoint (edge=open|closed).
  server.on("/api/shutters/set-edge", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("id", true) || !r->hasParam("edge", true)) {
      r->send(400, "application/json", "{\"error\":\"missing id/edge\"}"); return; }
    bool openEdge = r->getParam("edge", true)->value() == "open";
    if (!Shutters::setEdge(r->getParam("id", true)->value(), openEdge, ServoController::microseconds())) {
      r->send(400, "application/json", "{\"error\":\"unknown shutter\"}"); return; }
    Mqtt::shuttersChanged();          // calibration affects the reported position scale
    r->send(200, "application/json", Shutters::listJson());
  });
  // Snapshot the current position into a favourite (fav=daylight|privacy).
  server.on("/api/shutters/save-fav", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("id", true) || !r->hasParam("fav", true)) {
      r->send(400, "application/json", "{\"error\":\"missing id/fav\"}"); return; }
    bool privacy = r->getParam("fav", true)->value() == "privacy";
    if (!Shutters::saveFav(r->getParam("id", true)->value(), privacy, ServoController::microseconds())) {
      r->send(400, "application/json", "{\"error\":\"unknown shutter\"}"); return; }
    Mqtt::shuttersChanged();
    r->send(200, "application/json", Shutters::listJson());
  });
  // Move the servo to a stored position (fav=open|closed|daylight|privacy). Returns servo status.
  server.on("/api/shutters/recall", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!guard(r)) return;
    if (!r->hasParam("id", true) || !r->hasParam("fav", true)) {
      r->send(400, "application/json", "{\"error\":\"missing id/fav\"}"); return; }
    String id = r->getParam("id", true)->value(), fav = r->getParam("fav", true)->value();
    int us = (fav == "open" || fav == "closed") ? Shutters::edgeUs(id, fav == "open")
                                                 : Shutters::favUs(id, fav == "privacy");
    if (us == Shutters::UNSET) { r->send(400, "application/json", "{\"error\":\"position not set yet\"}"); return; }
    ServoController::writeUs(us);
    SolarLogic::notifyManualMove(id);   // a user recall pauses solar automation on this shutter for 2 h
    r->send(200, "application/json", ServoController::statusJson());
  });

  Ota::begin(server);   // /api/ota (firmware + filesystem)

  // ---- Static SPA from LittleFS, with an embedded recovery fallback ----
  if (LittleFS.exists("/index.html")) {
    // no-cache (not no-store): browsers must revalidate every load instead of trusting their
    // heuristic cache. LittleFS files report an epoch Last-Modified, so without this a browser
    // can cache index.html/app.js/style.css indefinitely — after reflashing a new FS image the
    // UI keeps running the OLD JS with old button wiring until a hard refresh clears it.
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("no-cache");
    LOGI("http", "serving web UI from LittleFS (no-cache: always revalidates)");
  } else {
    LOGW("http", "no /index.html in LittleFS — serving recovery page (flash the FS image)");
    server.onNotFound([](AsyncWebServerRequest *r) { r->send(200, "text/html", bootstrapPage()); });
  }

  server.begin();
  LOGI("http", "server up — http://%s.local or the device IP", host.c_str());
}

void loop() {
  Ota::loop();
  ws.cleanupClients();
  if (pendingWifiConnect) {
    pendingWifiConnect = false;
    WiFiSetup::connectTo(connSsid, connPass);   // blocks ~12–24s; async server keeps serving
  }
}

}  // namespace WebUI
