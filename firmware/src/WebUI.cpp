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

// Shared <head> + <style> for both pages (embedded; migrates to LittleFS in Phase 2).
static String pageHead(const String &title) {
  String h = F("<!doctype html><html lang=en><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'><title>");
  h += title;
  h += F("</title><style>"
    ":root{color-scheme:light dark}"
    "body{font-family:system-ui,sans-serif;max-width:36rem;margin:2rem auto;padding:0 1rem;line-height:1.5}"
    "h1{font-size:1.4rem;margin-bottom:.25rem}.sub{opacity:.6;margin-top:0}"
    "h2{font-size:1rem;margin:1.3rem 0 .3rem;opacity:.8}"
    "table{border-collapse:collapse;width:100%;margin:.5rem 0}"
    "td{padding:.35rem .5rem;border-bottom:1px solid #8884}td:first-child{opacity:.65;white-space:nowrap}"
    "td:last-child{text-align:right;font-variant-numeric:tabular-nums}"
    ".btn{display:inline-block;padding:.6rem 1rem;border:1px solid #8886;border-radius:.5rem;"
    "text-decoration:none;background:none;color:inherit;font:inherit;cursor:pointer}"
    ".btn:disabled{opacity:.5}.danger{border-color:#c0392b}"
    ".row{display:flex;gap:.5rem;flex-wrap:wrap;margin-top:.5rem}"
    ".tabs{display:flex;gap:.25rem;border-bottom:1px solid #8886;margin-top:1rem}"
    ".tab{padding:.55rem .9rem;border:none;background:none;color:inherit;font:inherit;cursor:pointer;"
    "border-bottom:2px solid transparent;opacity:.6}"
    ".tab:hover{opacity:.9}.tab.active{opacity:1;border-bottom-color:currentColor;font-weight:600}"
    ".pane{display:none;padding-top:1rem}.pane.active{display:block}"
    ".muted{opacity:.6}label{font-size:.9em;opacity:.75}input[type=file]{max-width:100%}"
    "footer{margin-top:2rem;padding-top:1rem;border-top:1px solid #8886}"
    "progress{width:100%;height:.6rem;display:none;margin-top:.5rem}"
    "</style></head><body>");
  return h;
}

// ---- Main page (tabbed) -----------------------------------------------------

static String statusPage() {
  String html = pageHead("Shutter Hub");
  html += F("<h1>ESP32 Shutter Hub</h1><p class=sub>Framework scaffold &middot; firmware v" FW_VERSION "</p>"
    "<div class=tabs>"
    "<button class='tab active' data-t=sys>System</button>"
    "<button class=tab data-t=fw>Firmware</button>"
    "<button class=tab data-t=home>Apple Home</button>"
    "</div>"
    "<section id=sys class='pane active'><h2>WiFi</h2><table>");
  html += "<tr><td>WiFi network</td><td>" + WiFi.SSID() + "</td></tr>";
  html += "<tr><td>IP address</td><td>" + WiFi.localIP().toString() + "</td></tr>";
  html += "<tr><td>MAC</td><td>" + WiFi.macAddress() + "</td></tr>";
  html += "<tr><td>Signal</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  html += F("</table><div class=row><a class=btn href='/wifi'>Change network</a></div>"
    "<h2>System</h2><table>");
  html += "<tr><td>Device name</td><td>" + AppConfig::deviceName() + "</td></tr>";
  html += "<tr><td>Firmware</td><td>v" FW_VERSION "</td></tr>";
  html += "<tr><td>Hostname</td><td>" + AppConfig::deviceName() + ".local</td></tr>";
  html += "<tr><td>Uptime</td><td>" + Diagnostics::humanUptime() + "</td></tr>";
  html += "<tr><td>Boot count</td><td>" + String(AppConfig::bootCount()) + "</td></tr>";
  html += "<tr><td>Free heap</td><td>" + String(ESP.getFreeHeap() / 1024) + " KB</td></tr>";
  html += "<tr><td>Last restart</td><td>" + Diagnostics::resetReason() + "</td></tr>";
  html += F("</table><div class=row><a class=btn href='/info' target=_blank>/info (JSON)</a></div>"
    "</section>"

    // ---- Firmware tab — custom OTA, three explicit actions ----
    "<section id=fw class=pane><table>"
    "<tr><td>Installed firmware</td><td>v" FW_VERSION "</td></tr>"
    "<tr><td>Last flash</td><td>");
  html += lastFlashText();
  html += F("</td></tr></table>"
    "<div class=field><label for=fwfile>Firmware image (.bin)</label><br>"
    "<input type=file id=fwfile accept='.bin'></div>"
    "<div class=field style='margin-top:.6rem'><label for=fsfile>Filesystem / LittleFS image (.bin)</label><br>"
    "<input type=file id=fsfile accept='.bin'></div>"
    "<div class=row>"
    "<button class=btn id=flashfw type=button>Flash firmware</button>"
    "<button class=btn id=flashfs type=button>Flash LittleFS</button>"
    "<button class=btn id=flashboth type=button>Flash both</button>"
    "</div>"
    "<progress id=otaprog value=0 max=100></progress>"
    "<p id=otastatus class=muted></p>"
    "<p class=muted style='font-size:.85em'>Firmware flashes reboot the hub; the filesystem flash does "
    "not. <b>Flash both</b> writes the filesystem first, then the firmware. Saved WiFi and settings are "
    "kept.</p></section>"

    // ---- Apple Home tab ----
    "<section id=home class=pane>"
    "<p class=muted>Apple Home (HomeKit) setup will appear here in a future release "
    "&mdash; nothing to configure yet.</p></section>"

    // ---- Bottom of page: Reset + Reboot, both with confirmation ----
    "<footer><div class=row>"
    "<form method=POST action='/forget-wifi' "
    "onsubmit=\"return confirm('RESET: forget the saved WiFi and restart into setup mode? "
    "You will need to reconnect the hub to your network.')\">"
    "<button class='btn danger' type=submit>Reset</button></form>"
    "<form method=POST action='/reboot' onsubmit=\"return confirm('Reboot the hub now?')\">"
    "<button class=btn type=submit>Reboot</button></form>"
    "</div></footer>"

    "<script>"
    // tab switching
    "document.querySelectorAll('.tab').forEach(function(b){b.addEventListener('click',function(){"
    "document.querySelectorAll('.tab').forEach(function(x){x.classList.toggle('active',x===b)});"
    "document.querySelectorAll('.pane').forEach(function(x){x.classList.toggle('active',x.id===b.dataset.t)});"
    "})});"
    // OTA: three explicit actions
    "(function(){"
    "var st=document.getElementById('otastatus'),pg=document.getElementById('otaprog');"
    "function set(m){st.textContent=m;}"
    "function busy(b){['flashfw','flashfs','flashboth'].forEach(function(i){document.getElementById(i).disabled=b;});}"
    "function fw(){return document.getElementById('fwfile').files[0];}"
    "function fs(){return document.getElementById('fsfile').files[0];}"
    "function up(t,f){return new Promise(function(res,rej){"
    "var x=new XMLHttpRequest();x.open('POST','/api/ota?target='+t);"
    "x.upload.onprogress=function(e){if(e.lengthComputable){pg.style.display='block';pg.value=Math.round(100*e.loaded/e.total);}};"
    "x.onload=function(){pg.style.display='none';var r={};try{r=JSON.parse(x.responseText);}catch(e){}"
    "if(x.status===200&&r.ok){res();}else{rej(r.error||('HTTP '+x.status));}};"
    "x.onerror=function(){pg.style.display='none';rej('network error');};"
    "var fd=new FormData();fd.append('file',f);set('Uploading '+t+'\\u2026');x.send(fd);});}"
    "document.getElementById('flashfw').addEventListener('click',function(){var f=fw();"
    "if(!f){set('Choose a firmware image first.');return;}busy(true);"
    "up('firmware',f).then(function(){set('Firmware flashed \\u2014 rebooting, reconnect in ~15s\\u2026');})"
    ".catch(function(e){set('Failed: '+e);}).then(function(){busy(false);});});"
    "document.getElementById('flashfs').addEventListener('click',function(){var f=fs();"
    "if(!f){set('Choose a filesystem image first.');return;}busy(true);"
    "up('filesystem',f).then(function(){set('Filesystem flashed. Reboot to apply.');})"
    ".catch(function(e){set('Failed: '+e);}).then(function(){busy(false);});});"
    "document.getElementById('flashboth').addEventListener('click',function(){var a=fw(),b=fs();"
    "if(!a||!b){set('Choose both a firmware and a filesystem image.');return;}busy(true);"
    "up('filesystem',b).then(function(){set('Filesystem done; flashing firmware\\u2026');return up('firmware',a);})"
    ".then(function(){set('Both flashed \\u2014 rebooting, reconnect in ~15s\\u2026');})"
    ".catch(function(e){set('Failed: '+e);}).then(function(){busy(false);});});"
    "})();"
    "</script></body></html>");
  return html;
}

// ---- Dedicated "Change network" page ----------------------------------------

static String wifiPage() {
  String html = pageHead("Change WiFi — Shutter Hub");
  html += F("<h1>Change WiFi network</h1><p><a class=btn href='/'>&larr; Back</a></p><table>");
  html += "<tr><td>Current network</td><td>" + WiFi.SSID() + "</td></tr>";
  html += "<tr><td>IP address</td><td>" + WiFi.localIP().toString() + "</td></tr>";
  html += F("</table>"
    "<div class=field style='margin-top:1rem'><label for=ssidsel>Network</label><br>"
    "<select id=ssidsel><option value=''>-- scan for networks --</option></select> "
    "<button class=btn id=scanbtn type=button>Scan</button></div>"
    "<div class=field style='margin-top:.5rem'>"
    "<input type=password id=wifipass placeholder='WiFi password'> "
    "<button class=btn id=setbtn type=button>Set</button></div>"
    "<p id=wifistatus class=muted></p>"
    "<p class=muted style='font-size:.85em'>Pick a network and enter its password to switch. If it's a "
    "different network from the one you're on now, the hub moves to it and you may need to reconnect "
    "from a device on that network (find it at shutter-hub.local). A wrong password reverts to the "
    "current network.</p>"
    "<script>(function(){"
    "var ss=document.getElementById('ssidsel'),sb=document.getElementById('scanbtn'),"
    "cb=document.getElementById('setbtn'),ws=document.getElementById('wifistatus'),"
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
    "if(!confirm('Switch the hub to \"'+s+'\"?'))return;"
    "cb.disabled=true;w('Connecting to '+s+'\\u2026');"
    "var fd=new FormData();fd.append('ssid',s);fd.append('pass',wp.value);"
    "fetch('/api/wifi/connect',{method:'POST',body:fd}).then(function(r){return r.text();})"
    ".then(function(t){w(t);cb.disabled=false;})"
    ".catch(function(e){w('The hub may have switched networks — reconnect and open shutter-hub.local.');cb.disabled=false;});"
    "});scan();})();"
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
  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/html", wifiPage());
  });
  server.on("/healthz", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/plain", "ok");
  });
  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "application/json", Diagnostics::infoJson());
  });
  // Reset: forget WiFi and reboot into the setup portal. Deferred to loop().
  server.on("/forget-wifi", HTTP_POST, [](AsyncWebServerRequest *r) {
    r->send(200, "text/html",
      "<meta http-equiv=refresh content='10;url=/'>"
      "<p style='font-family:system-ui'>Reset — WiFi cleared. The hub is restarting as "
      "<b>Shutter-Hub-Setup</b>; join that network to set it up again.</p>");
    pendingForget = true;
  });
  server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *r) {
    r->send(200, "text/html",
      "<meta http-equiv=refresh content='8;url=/'>"
      "<p style='font-family:system-ui'>Rebooting&hellip;</p>");
    pendingReboot = true;
  });
  // Scan for nearby networks (async so the request doesn't block). 202 = scanning.
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
    WiFi.scanDelete();
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
  if (pendingWifiConnect) {
    pendingWifiConnect = false;
    WiFiSetup::connectTo(connSsid, connPass);   // blocks ~12–24s; async server keeps serving
  }
  if (pendingForget) { delay(400); WiFiSetup::forgetAndReboot(); }
  if (pendingReboot) { delay(400); Diagnostics::reboot(); }
}

}  // namespace WebUI
