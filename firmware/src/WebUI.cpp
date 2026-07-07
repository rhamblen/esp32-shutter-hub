#include "WebUI.h"
#include "AppConfig.h"
#include "Diagnostics.h"
#include "WiFiSetup.h"
#include "Ota.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <time.h>

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

static AsyncWebServer server(80);
static bool   pendingReboot      = false;
static bool   pendingForget      = false;
static bool   pendingApApply     = false;
static bool   apDesired          = false;
static bool   pendingWifiConnect = false;
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

// Format a stored flash timestamp for display (UTC; 0 = clock never synced).
static String fmtWhen(uint32_t epoch) {
  if (epoch == 0) return "time not synced";
  time_t t = (time_t)epoch;
  struct tm tmv;
  gmtime_r(&t, &tmv);
  char b[32];
  strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S UTC", &tmv);
  return String(b);
}

static String lastFlashText() {
  if (AppConfig::lastFlashType() == "none") return "none yet";
  return AppConfig::lastFlashType() + " — " + (AppConfig::lastFlashOk() ? "OK" : "FAILED") +
         " — " + fmtWhen(AppConfig::lastFlashEpoch());
}

// ---- Tabbed status page (embedded; migrates to LittleFS in Phase 2) ----------

static String statusPage() {
  String html = F(
    "<!doctype html><html lang=en><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Shutter Hub</title><style>"
    ":root{color-scheme:light dark}"
    "body{font-family:system-ui,sans-serif;max-width:36rem;margin:2rem auto;padding:0 1rem;line-height:1.5}"
    "h1{font-size:1.4rem;margin-bottom:.25rem}.sub{opacity:.6;margin-top:0}"
    "h2{font-size:1rem;margin:1.3rem 0 .3rem;opacity:.8}"
    "table{border-collapse:collapse;width:100%;margin:.5rem 0}"
    "td{padding:.35rem .5rem;border-bottom:1px solid #8884}td:first-child{opacity:.65;white-space:nowrap}"
    "td:last-child{text-align:right;font-variant-numeric:tabular-nums}"
    ".btn{display:inline-block;padding:.6rem 1rem;border:1px solid #8886;border-radius:.5rem;"
    "text-decoration:none;background:none;color:inherit;font:inherit;cursor:pointer}"
    ".row{display:flex;gap:.5rem;flex-wrap:wrap;margin-top:.5rem}"
    ".tabs{display:flex;gap:.25rem;border-bottom:1px solid #8886;margin-top:1rem}"
    ".tab{padding:.55rem .9rem;border:none;background:none;color:inherit;font:inherit;cursor:pointer;"
    "border-bottom:2px solid transparent;opacity:.6}"
    ".tab:hover{opacity:.9}.tab.active{opacity:1;border-bottom-color:currentColor;font-weight:600}"
    ".pane{display:none;padding-top:1rem}.pane.active{display:block}"
    ".muted{opacity:.6}"
    "label{font-size:.9em;opacity:.75}input[type=file]{max-width:100%}"
    "progress{width:100%;height:.6rem;display:none;margin-top:.5rem}"
    "</style></head><body>"
    "<h1>ESP32 Shutter Hub</h1><p class=sub>Framework scaffold &middot; firmware v" FW_VERSION "</p>"
    "<div class=tabs>"
    "<button class='tab active' data-t=sys>System</button>"
    "<button class=tab data-t=fw>Firmware</button>"
    "<button class=tab data-t=home>Apple Home</button>"
    "</div>"
    "<section id=sys class='pane active'><h2>WiFi</h2><table>");
  bool ap = AppConfig::apEnabled();
  html += "<tr><td>WiFi network</td><td>" + WiFi.SSID() + "</td></tr>";
  html += "<tr><td>IP address</td><td>" + WiFi.localIP().toString() + "</td></tr>";
  html += "<tr><td>MAC</td><td>" + WiFi.macAddress() + "</td></tr>";
  html += "<tr><td>Signal</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  html += "<tr><td>Access point</td><td>" + String(ap ? "on (192.168.4.1)" : "off") + "</td></tr>";
  html += F("</table>"
    "<div class=field style='margin-top:.6rem'><label for=ssidsel>Change network</label><br>"
    "<select id=ssidsel><option value=''>-- scan for networks --</option></select> "
    "<button class=btn id=scanbtn type=button>Scan</button></div>"
    "<div class=field style='margin-top:.5rem'>"
    "<input type=password id=wifipass placeholder='WiFi password'> "
    "<button class=btn id=connbtn type=button>Connect</button></div>"
    "<p id=wifistatus class=muted></p>"
    "<div class=row>");
  html += "<form method=POST action='/ap'><input type=hidden name=on value='" +
          String(ap ? "0" : "1") + "'><button class=btn type=submit>" +
          String(ap ? "Disable" : "Enable") + " access point</button></form>";
  html += F("<form method=POST action='/forget-wifi' "
    "onsubmit=\"return confirm('Forget saved WiFi and reboot into the setup portal?')\">"
    "<button class=btn type=submit>Re-run setup</button></form></div>"
    "<p class=muted style='font-size:.85em'>Pick a network and enter its password to switch. If it's a "
    "different network from the one you're on now, the hub moves to it and you may need to reconnect "
    "from a device on that network (find it at shutter-hub.local). A wrong password reverts to the "
    "current network. <b>Re-run setup</b> reboots into the setup portal; the access point lets you "
    "reach this page at 192.168.4.1 without your WiFi.</p>"
    "<h2>System</h2><table>");
  html += "<tr><td>Device name</td><td>" + AppConfig::deviceName() + "</td></tr>";
  html += "<tr><td>Firmware</td><td>v" FW_VERSION "</td></tr>";
  html += "<tr><td>Hostname</td><td>" + AppConfig::deviceName() + ".local</td></tr>";
  html += "<tr><td>Uptime</td><td>" + Diagnostics::humanUptime() + "</td></tr>";
  html += "<tr><td>Boot count</td><td>" + String(AppConfig::bootCount()) + "</td></tr>";
  html += "<tr><td>Free heap</td><td>" + String(ESP.getFreeHeap() / 1024) + " KB</td></tr>";
  html += "<tr><td>Last restart</td><td>" + Diagnostics::resetReason() + "</td></tr>";
  html += F("</table><div class=row>"
    "<form method=POST action='/reboot' onsubmit=\"return confirm('Restart the hub now?')\">"
    "<button class=btn type=submit>Restart hub</button></form>"
    "<a class=btn href='/info' target=_blank>/info (JSON)</a>"
    "</div></section>");

  // ---- Firmware tab — custom OTA (firmware + LittleFS, independent or together) ----
  html += F("<section id=fw class=pane><table>"
    "<tr><td>Installed firmware</td><td>v" FW_VERSION "</td></tr>"
    "<tr><td>Last flash</td><td>");
  html += lastFlashText();
  html += F("</td></tr></table>"
    "<p class=muted>Choose a firmware image, a filesystem (LittleFS) image, or both. If both are "
    "selected the filesystem is flashed first, then the firmware (which reboots). Saved WiFi and "
    "settings are kept.</p>"
    "<div class=field><label for=fwfile>Firmware image (.bin)</label><br>"
    "<input type=file id=fwfile accept='.bin'></div>"
    "<div class=field style='margin-top:.6rem'><label for=fsfile>Filesystem / LittleFS image (.bin)</label><br>"
    "<input type=file id=fsfile accept='.bin'></div>"
    "<div class=row><button class=btn id=flashbtn type=button>Flash selected</button></div>"
    "<progress id=otaprog value=0 max=100></progress>"
    "<p id=otastatus class=muted></p>"
    "</section>"

    "<section id=home class=pane>"
    "<p class=muted>Apple Home (HomeKit) setup will appear here in a future release "
    "&mdash; nothing to configure yet.</p>"
    "</section>"

    "<script>"
    "document.querySelectorAll('.tab').forEach(function(b){b.addEventListener('click',function(){"
    "document.querySelectorAll('.tab').forEach(function(x){x.classList.toggle('active',x===b)});"
    "document.querySelectorAll('.pane').forEach(function(x){x.classList.toggle('active',x.id===b.dataset.t)});"
    "})});"
    "(function(){"
    "var st=document.getElementById('otastatus'),pg=document.getElementById('otaprog'),bt=document.getElementById('flashbtn');"
    "function set(m){st.textContent=m;}"
    "function up(t,f){return new Promise(function(res,rej){"
    "var x=new XMLHttpRequest();x.open('POST','/api/ota?target='+t);"
    "x.upload.onprogress=function(e){if(e.lengthComputable){pg.style.display='block';pg.value=Math.round(100*e.loaded/e.total);}};"
    "x.onload=function(){pg.style.display='none';var r={};try{r=JSON.parse(x.responseText);}catch(e){}"
    "if(x.status===200&&r.ok){res();}else{rej(r.error||('HTTP '+x.status));}};"
    "x.onerror=function(){pg.style.display='none';rej('network error');};"
    "var fd=new FormData();fd.append('file',f);set('Uploading '+t+'\\u2026');x.send(fd);});}"
    "bt.addEventListener('click',function(){"
    "var fw=document.getElementById('fwfile').files[0],fs=document.getElementById('fsfile').files[0];"
    "if(!fw&&!fs){set('Choose a firmware and/or filesystem image first.');return;}"
    "bt.disabled=true;var c=Promise.resolve();"
    "if(fs)c=c.then(function(){return up('filesystem',fs);}).then(function(){set('Filesystem flashed.');});"
    "if(fw)c=c.then(function(){return up('firmware',fw);}).then(function(){set('Firmware flashed \\u2014 rebooting, reconnect in ~15s\\u2026');});"
    "else c=c.then(function(){set('Filesystem flashed. Reboot to apply.');});"
    "c.catch(function(e){set('Failed: '+e);}).then(function(){bt.disabled=false;});"
    "});})();"
    // WiFi scan + change-network
    "(function(){"
    "var ss=document.getElementById('ssidsel'),sb=document.getElementById('scanbtn'),"
    "cb=document.getElementById('connbtn'),ws=document.getElementById('wifistatus'),"
    "wp=document.getElementById('wifipass');"
    "function w(m){ws.textContent=m;}"
    "function scan(){w('Scanning\\u2026');sb.disabled=true;"
    "fetch('/api/wifi/scan').then(function(r){return r.json().then(function(j){return{s:r.status,j:j};});}).then(function(o){"
    "if(o.s===202||(o.j&&o.j.scanning)){setTimeout(scan,1500);return;}"
    "ss.innerHTML='<option value=\"\">-- select network --</option>';"
    "o.j.sort(function(a,b){return b.rssi-a.rssi;}).forEach(function(n){var op=document.createElement('option');"
    "op.value=n.ssid;op.textContent=n.ssid+(n.lock?' [locked]':'')+' ('+n.rssi+'dBm)';ss.appendChild(op);});"
    "sb.disabled=false;w(o.j.length+' network(s) found.');"
    "}).catch(function(e){sb.disabled=false;w('Scan failed: '+e);});}"
    "sb.addEventListener('click',scan);"
    "cb.addEventListener('click',function(){var s=ss.value;if(!s){w('Choose a network first.');return;}"
    "cb.disabled=true;w('Connecting to '+s+'\\u2026');"
    "var fd=new FormData();fd.append('ssid',s);fd.append('pass',wp.value);"
    "fetch('/api/wifi/connect',{method:'POST',body:fd}).then(function(r){return r.text();})"
    ".then(function(t){w(t);cb.disabled=false;})"
    ".catch(function(e){w('The hub may have switched networks — reconnect and refresh.');cb.disabled=false;});"
    "});})();"
    "</script></body></html>");
  return html;
}

// ---- Public API -------------------------------------------------------------

namespace WebUI {

void begin() {
  String host = AppConfig::deviceName();

  if (MDNS.begin(host.c_str())) {
    MDNS.addService("http", "tcp", 80);
    LOGI("mdns", "http://%s.local", host.c_str());
  } else {
    LOGW("mdns", "start failed");
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/html", statusPage());
  });
  server.on("/healthz", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", "ok");
  });
  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "application/json", Diagnostics::infoJson());
  });
  // Deferred to loop() so the HTTP response flushes before we restart.
  server.on("/forget-wifi", HTTP_POST, [](AsyncWebServerRequest *r) {
    r->send(200, "text/html",
      "<meta http-equiv=refresh content='10;url=/'>"
      "<p style='font-family:system-ui'>WiFi cleared. The hub is restarting as "
      "<b>Shutter-Hub-Setup</b> — join that network to choose a new WiFi.</p>");
    pendingForget = true;
  });
  server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *r) {
    r->send(200, "text/html",
      "<meta http-equiv=refresh content='8;url=/'>"
      "<p style='font-family:system-ui'>Rebooting&hellip;</p>");
    pendingReboot = true;
  });
  // Enable/disable the management access point. Applied in loop() (off the async
  // task) since it changes WiFi mode.
  server.on("/ap", HTTP_POST, [](AsyncWebServerRequest *r) {
    bool on = r->hasParam("on", true) && r->getParam("on", true)->value() == "1";
    r->send(200, "text/html",
      String("<meta http-equiv=refresh content='2;url=/'><p style='font-family:system-ui'>"
             "Access point ") + (on ? "enabled" : "disabled") + ".</p>");
    apDesired      = on;
    pendingApApply = true;
  });
  // Scan for nearby networks (async so the request doesn't block). Poll until it
  // returns the array; 202 = still scanning.
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *r) {
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
    WiFi.scanDelete();          // free results; next scan starts fresh
    r->send(200, "application/json", j);
  });
  // Switch to a chosen network (applied in loop() — it blocks while joining).
  server.on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!r->hasParam("ssid", true)) { r->send(400, "text/plain", "missing ssid"); return; }
    connSsid = r->getParam("ssid", true)->value();
    connPass = r->hasParam("pass", true) ? r->getParam("pass", true)->value() : "";
    pendingWifiConnect = true;
    r->send(200, "text/plain",
      "Connecting to \"" + connSsid + "\"… If the password is right the hub moves to that "
      "network (reconnect there, e.g. shutter-hub.local). A wrong password reverts to the current "
      "network — refresh in ~20s.");
  });

  Ota::begin(server);   // /api/ota
  server.begin();
  LOGI("http", "server up — http://%s.local or the device IP", host.c_str());
}

void loop() {
  Ota::loop();
  if (pendingApApply) {
    pendingApApply = false;
    delay(300);                       // let the HTTP response flush
    AppConfig::setApEnabled(apDesired);
    WiFiSetup::setSoftAP(apDesired);
  }
  if (pendingWifiConnect) {
    pendingWifiConnect = false;
    WiFiSetup::connectTo(connSsid, connPass);   // blocks ~12–24s; async server keeps serving
  }
  if (pendingForget) { delay(400); WiFiSetup::forgetAndReboot(); }
  if (pendingReboot) { delay(400); Diagnostics::reboot(); }
}

}  // namespace WebUI
