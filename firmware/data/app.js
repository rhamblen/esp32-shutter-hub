"use strict";
// Shutter Hub web UI — vanilla SPA. Talks to the firmware JSON API + /ws/logs.

const $  = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => [...r.querySelectorAll(s)];

async function apiGet(u) { const r = await fetch(u); if (!r.ok) throw new Error("HTTP " + r.status); return r.json(); }
async function apiPost(u, obj) {
  const body = obj ? new URLSearchParams(obj) : undefined;
  const r = await fetch(u, { method: "POST",
    headers: obj ? { "Content-Type": "application/x-www-form-urlencoded" } : {}, body });
  let j = {}; try { j = await r.json(); } catch (e) {}
  if (!r.ok) throw new Error(j.error || ("HTTP " + r.status));
  return j;
}
const fmtKB = b => (b / 1024).toFixed(1) + " KB";
const rssiTxt = r => r + " dBm" + (r >= -60 ? " (good)" : r >= -75 ? " (ok)" : " (poor)");

// ---- Routing ----------------------------------------------------------------
const routes = ["info", "mqtt", "actions", "shutters", "system", "ota", "logs"];
function go(route) {
  if (!routes.includes(route)) route = "info";
  $$(".nav-item").forEach(n => n.classList.toggle("active", n.dataset.route === route));
  $$(".page").forEach(p => p.classList.toggle("hidden", p.id !== "page-" + route));
  location.hash = route;
  if (route === "info")    loadInfo();
  if (route === "mqtt")    loadMqtt();
  if (route === "actions") svRefresh();
  if (route === "shutters") shOnShow();
  if (route === "system")  loadSystem();
  if (route === "ota")     loadOta();
  if (route === "logs")    logsOnShow();
}
$$(".nav-item").forEach(n => n.addEventListener("click", () => go(n.dataset.route)));
window.addEventListener("hashchange", () => go(location.hash.slice(1)));

// Generic sub-tab handler (MQTT + System)
$$(".tabs").forEach(bar => {
  bar.addEventListener("click", e => {
    const b = e.target.closest(".tab"); if (!b) return;
    const scope = bar.parentElement;
    $$(".tab", bar).forEach(t => t.classList.toggle("active", t === b));
    $$(".tabpane", scope).forEach(p => p.classList.toggle("hidden", p.dataset.pane !== b.dataset.tab));
  });
});

// ---- Info -------------------------------------------------------------------
let infoTimer = null;
async function loadInfo() {
  try {
    const d = await apiGet("/api/info");
    $("#brandName").textContent = d.device;
    $("#sideFw").textContent = "v" + d.fw;
    $("#brandState").textContent = d.wifi.connected ? "Online" : "Offline";
    $("#brandDot").classList.toggle("off", !d.wifi.connected);
    $("#i_fw").textContent = "v" + d.fw;
    $("#i_variant").textContent = d.variant || "—";
    $("#i_dev").textContent = d.device;
    $("#i_host").textContent = d.host;
    $("#i_chip").textContent = d.chip;
    $("#i_up").textContent = d.uptime;
    $("#i_boot").textContent = d.boot_count;
    $("#i_reset").textContent = d.reset_reason;
    $("#i_ssid").textContent = d.wifi.ssid || "—";
    $("#i_ip").textContent = d.wifi.ip;
    $("#i_mac").textContent = d.wifi.mac;
    $("#i_rssi").textContent = rssiTxt(d.wifi.rssi);
    $("#i_heap").textContent = fmtKB(d.free_heap);
    $("#i_mqtt").textContent = d.mqtt.enabled ? (d.mqtt.connected ? "Connected" : d.mqtt.state) : "Disabled";
  } catch (e) {}
  clearTimeout(infoTimer);
  if ($("#page-info").offsetParent !== null) infoTimer = setTimeout(loadInfo, 5000);
}

// ---- MQTT -------------------------------------------------------------------
let mqShutters = [];   // cached /api/shutters for the Topics tab

function mqttPill(d) {
  const p = $("#mq_pill");
  const ok = d.connected, on = d.enabled;
  p.style.color = ok ? "var(--green)" : on ? "var(--amber)" : "var(--muted)";
  p.querySelector(".dot").classList.toggle("off", !ok);
  p.querySelector("span:last-child").textContent =
    ok ? "Connected" : on ? (d.state || "connecting") : "Disabled";
  renderTopics();
}
// Live topic map on the Topics sub-tab: hub-wide rows + one block per shutter.
function renderTopics() {
  const base = $("#mq_base").value.trim() || "shutter-hub";
  $("#t_status").textContent = base + "/status";
  $("#t_state").textContent  = base + "/state";
  $("#t_none").classList.toggle("hidden", mqShutters.length > 0);
  const row = (label, dir, topic, hint) =>
    `<div class="kv"><span>${label} <i class="tdir">${dir}</i>${hint ? `<small class="thint">${hint}</small>` : ""}</span><b class="mono">${topic}</b></div>`;
  $("#t_shutters").innerHTML = mqShutters.map(s => {
    const b = `${base}/cover/${s.id}`;
    return `<div class="tsh"><div class="tsh-name">${esc(s.name)} <span class="muted">· ${s.id}</span></div>` +
      row("Command", "→ hub", `${b}/set`, "OPEN · CLOSE · STOP") +
      row("Position target", "→ hub", `${b}/position/set`, "0–100") +
      row("Custom command", "→ hub", `${b}/cmd`, "jog_open · jog_close · recall:daylight · recall:privacy · save:daylight · save:privacy") +
      row("Position", "hub →", `${b}/position`, "0–100, retained") +
      row("State", "hub →", `${b}/state`, "opening · closing · open · closed · stopped") +
      `</div>`;
  }).join("");
}
async function loadMqtt() {
  try {
    const d = await apiGet("/api/mqtt");
    $("#mq_host").value = d.host; $("#mq_port").value = d.port;
    $("#mq_client").value = d.clientId; $("#mq_user").value = d.user;
    $("#mq_pass").value = ""; $("#mq_pass").placeholder = d.hasPass ? "•••••• (unchanged)" : "password";
    $("#mq_base").value = d.base; $("#mq_ha").checked = d.haDiscovery; $("#mq_en").checked = d.enabled;
    try { mqShutters = await apiGet("/api/shutters"); } catch (e) { mqShutters = []; }
    mqttPill(d);
  } catch (e) { $("#mq_msg").textContent = "Load failed: " + e.message; }
}
$("#mq_base").addEventListener("input", renderTopics);
$("#mq_save").addEventListener("click", async () => {
  $("#mq_msg").textContent = "Saving…";
  try {
    const d = await apiPost("/api/mqtt", {
      enabled: $("#mq_en").checked, host: $("#mq_host").value.trim(), port: $("#mq_port").value,
      clientId: $("#mq_client").value.trim(), user: $("#mq_user").value, pass: $("#mq_pass").value,
      base: $("#mq_base").value.trim(), haDiscovery: $("#mq_ha").checked });
    $("#mq_pass").value = ""; mqttPill(d);
    $("#mq_msg").textContent = "Saved — applying…";
    setTimeout(loadMqtt, 1500);
  } catch (e) { $("#mq_msg").textContent = "Failed: " + e.message; }
});
$("#mq_reset").addEventListener("click", loadMqtt);

// ---- System -----------------------------------------------------------------
async function loadSystem() {
  try {
    const d = await apiGet("/api/info");
    $("#sy_dev").textContent = d.device; $("#sy_host").textContent = d.host;
    $("#sy_fw").textContent = "v" + d.fw;
    $("#wf_cur").textContent = d.wifi.ssid || "—"; $("#wf_ip").textContent = d.wifi.ip;
  } catch (e) {}
  try {
    const a = await apiGet("/api/auth");
    $("#se_en").checked = a.enabled; $("#se_user").value = a.user;
  } catch (e) {}
  loadHomekit();
}
function qa(id, url, confirmMsg) {
  $(id).addEventListener("click", async () => {
    if (confirmMsg && !confirm(confirmMsg)) return;
    $("#qa_msg").textContent = "Working…";
    try { const j = await apiPost(url); $("#qa_msg").textContent = j.msg || "OK"; }
    catch (e) { $("#qa_msg").textContent = "Failed: " + e.message; }
  });
}
qa("#qa_reboot", "/api/system/reboot", "Reboot the hub now?");
qa("#qa_wifi", "/api/system/reset-wifi",
   "RESET WIFI: forget the saved network and restart into Shutter-Hub-Setup? You'll reconnect the hub afterwards.");
qa("#qa_cfg", "/api/system/reset-config",
   "RESET CONFIG: clear device name, servo pin, MQTT, HomeKit and web-auth settings (WiFi is kept) and reboot?");

$("#wf_scan").addEventListener("click", scanWifi);
async function scanWifi() {
  const sel = $("#wf_sel"); $("#wf_msg").textContent = "Scanning…"; $("#wf_scan").disabled = true;
  try {
    let r = await fetch("/api/wifi/scan"); let j = await r.json();
    while (r.status === 202 || j.scanning) { await new Promise(s => setTimeout(s, 1500));
      r = await fetch("/api/wifi/scan"); j = await r.json(); }
    sel.innerHTML = '<option value="">— select network —</option>';
    j.sort((a, b) => b.rssi - a.rssi).forEach(n => {
      const o = document.createElement("option");
      o.value = n.ssid; o.textContent = `${n.ssid}${n.lock ? " 🔒" : ""} (${n.rssi}dBm)`; sel.appendChild(o);
    });
    $("#wf_msg").textContent = j.length + " network(s) found.";
  } catch (e) { $("#wf_msg").textContent = "Scan failed: " + e.message; }
  $("#wf_scan").disabled = false;
}
$("#wf_connect").addEventListener("click", async () => {
  const ssid = $("#wf_sel").value;
  if (!ssid) { $("#wf_msg").textContent = "Choose a network first."; return; }
  if (!confirm(`Switch the hub to "${ssid}"?`)) return;
  $("#wf_msg").textContent = "Connecting…";
  try { const j = await apiPost("/api/wifi/connect", { ssid, pass: $("#wf_pass").value });
    $("#wf_msg").textContent = j.msg || "Connecting…"; }
  catch (e) { $("#wf_msg").textContent = "The hub may have switched networks — reconnect at its .local name."; }
});
$("#se_save").addEventListener("click", async () => {
  $("#se_msg").textContent = "Saving…";
  try { await apiPost("/api/auth",
      { enabled: $("#se_en").checked, user: $("#se_user").value.trim(), pass: $("#se_pass").value });
    $("#se_pass").value = "";
    $("#se_msg").textContent = "Saved. If enabled, the browser will ask for login on the next request.";
  } catch (e) { $("#se_msg").textContent = "Failed: " + e.message; }
});

// ---- HomeKit (System > HomeKit sub-tab) --------------------------------------
// Config is stored now; the HomeSpan bridge itself lands with the v0.5.0 firmware
// (the API reports `running:false` until then, which drives the status text below).
let hkStat = { running: false, paired: false };
const hkDigits = v => v.replace(/\D/g, "").slice(0, 8);
const hkFmt = c => c.length === 8 ? `${c.slice(0, 3)}-${c.slice(3, 5)}-${c.slice(5)}` : c;
// Apple X-HM:// pairing payload: category 2 (bridge) + IP flag + the code, base-36,
// then the 4-char setup ID. "SHUT" must match homeSpan.setQRID() when Phase 5 lands.
function hkQrUri(code) {
  const v = 2 * 2 ** 31 + 2 * 2 ** 27 + parseInt(code, 10);
  return "X-HM://" + v.toString(36).toUpperCase().padStart(9, "0") + "SHUT";
}
let hkLastUri = "";
function hkRenderQr() {
  const code = hkDigits($("#hk_code").value);
  const show = $("#hk_en").checked && !hkStat.paired && code.length === 8 && typeof QRCode !== "undefined";
  $("#hk_qrbox").classList.toggle("hidden", !show);
  if (!show) { hkLastUri = ""; return; }
  const uri = hkQrUri(code);
  $("#hk_qrcap").innerHTML = (hkStat.running
    ? "Scan with the iPhone camera or the Home app to pair"
    : "Pairing QR preview — scannable once the v0.5.0 bridge is running")
    + `<br><b>${hkFmt(code)}</b>`;
  if (uri === hkLastUri) return;
  hkLastUri = uri;
  $("#hk_qr").innerHTML = "";
  new QRCode($("#hk_qr"), { text: uri, width: 148, height: 148,
    colorDark: "#0b0d13", colorLight: "#ffffff", correctLevel: QRCode.CorrectLevel.Q });
}
async function loadHomekit() {
  try {
    const d = await apiGet("/api/homekit");
    hkStat = d;
    $("#hk_en").checked = d.enabled;
    $("#hk_name").value = d.name;
    $("#hk_code").value = hkFmt(d.code);
    $("#hk_run").textContent = d.running ? "Running" : "Not in this firmware yet — arrives with v0.5.0";
    $("#hk_pair").textContent = d.running
      ? (d.paired ? `Paired — ${d.controllers} controller(s)` : "Not paired") : "—";
    $("#hk_reset").disabled = !d.running;
    hkRenderQr();
  } catch (e) { $("#hk_msg").textContent = "Load failed: " + e.message; }
}
$("#hk_code").addEventListener("input", () => {
  $("#hk_code").value = hkFmt(hkDigits($("#hk_code").value)); hkRenderQr(); });
$("#hk_en").addEventListener("change", hkRenderQr);
$("#hk_rand").addEventListener("click", () => {
  let c;
  do { c = String(Math.floor(Math.random() * 1e8)).padStart(8, "0"); }
  while (/^(\d)\1{7}$/.test(c) || c === "12345678" || c === "87654321");
  $("#hk_code").value = hkFmt(c); hkRenderQr();
});
$("#hk_save").addEventListener("click", async () => {
  const code = hkDigits($("#hk_code").value);
  if (code.length !== 8) { $("#hk_msg").textContent = "Setup code must be 8 digits."; return; }
  $("#hk_msg").textContent = "Saving…";
  try {
    const d = await apiPost("/api/homekit",
      { enabled: $("#hk_en").checked, name: $("#hk_name").value.trim(), code });
    hkStat = d;
    $("#hk_msg").textContent = d.running
      ? "Saved — reboot to apply." : "Saved — applies when the v0.5.0 bridge firmware is flashed.";
    hkRenderQr();
  } catch (e) { $("#hk_msg").textContent = "Failed: " + e.message; }
});
$("#hk_reset").addEventListener("click", async () => {
  if (!confirm("Reset HomeKit pairings? Every paired iPhone / Home hub will have to re-pair.")) return;
  try { const j = await apiPost("/api/homekit/reset-pairings");
    $("#hk_msg").textContent = j.msg || "OK"; loadHomekit(); }
  catch (e) { $("#hk_msg").textContent = "Failed: " + e.message; }
});

// ---- Actions (servo bench test) --------------------------------------------
// The backend is a build-time choice (direct GPIO vs PCA9685); the status JSON
// carries `usesPca`, so this one page renders the right addressing controls.
const STRAP = [0, 2, 12, 15];
const svSpeedLabel = v => v + " °/s";
function svRender(s) {
  const pca = !!s.usesPca;
  $("#sv_gpio_block").classList.toggle("hidden", pca);
  $("#sv_pca_block").classList.toggle("hidden", !pca);
  $("#sv_bus_row").classList.toggle("hidden", !pca);
  if (pca) {
    $("#sv_headnote").textContent = "PCA9685 @0x40 · power servos from 5 V, common ground";
    $("#sv_addr_label").textContent = "PCA9685 channel";
    $("#sv_pinv").textContent = "CH" + s.channel;
    $("#sv_busv").textContent = "SDA GPIO" + s.sda + " · SCL GPIO" + s.scl;
    if ($("#sv_ch").value === "")  $("#sv_ch").value = s.channel;
    if ($("#sv_sda").value === "") $("#sv_sda").value = s.sda;
    if ($("#sv_scl").value === "") $("#sv_scl").value = s.scl;
  } else {
    $("#sv_headnote").textContent = "Default GPIO13 · power from 5 V, common ground";
    $("#sv_addr_label").textContent = "Signal GPIO";
    $("#sv_pinv").textContent = "GPIO" + s.pin;
    if ($("#sv_pin").value === "") $("#sv_pin").value = s.pin;
    $("#sv_pinwarn").textContent = STRAP.includes(s.pin)
      ? `Note: GPIO${s.pin} is a strapping/boot pin — fine for a quick test, avoid for the final build.` : "";
  }
  $("#sv_state").textContent = s.attached
    ? (s.sweeping ? "sweeping…" : (s.moving ? `moving → ${s.target}°` : "attached"))
    : "detached (released)";
  $("#sv_angle").textContent = s.angle + "°";
  $("#sv_us").textContent = s.us + " µs";
  $("#sv_speedv").textContent = s.speed === 0 ? "Max (instant)" : s.speed + " °/s";
  if (document.activeElement !== $("#sv_slider")) $("#sv_slider").value = s.moving ? s.target : s.angle;
  if (document.activeElement !== $("#sv_speed")) $("#sv_speed").value = Math.min(s.speed || 120, 120);
  svBusy = !!(s.moving || s.sweeping);
  $("#sv_sweep").textContent = s.sweeping ? "Stop sweep" : "Sweep";
}
async function svRefresh() { try { svRender(await apiGet("/api/servo")); } catch (e) {} }
async function svPost(u) { try { const j = await apiPost(u); svRender(j); $("#sv_msg").textContent = ""; }
  catch (e) { $("#sv_msg").textContent = "Failed: " + e.message; } }
$("#sv_setpin").addEventListener("click", () => {
  const g = $("#sv_pin").value; if (g === "") { $("#sv_msg").textContent = "Enter a GPIO first."; return; }
  svPost("/api/servo/pin?gpio=" + g);
});
$("#sv_setch").addEventListener("click", () => {
  const c = $("#sv_ch").value; if (c === "") { $("#sv_msg").textContent = "Enter a channel (0–15) first."; return; }
  svPost("/api/servo/channel?ch=" + c);
});
$("#sv_seti2c").addEventListener("click", () => {
  const sda = $("#sv_sda").value, scl = $("#sv_scl").value;
  if (sda === "" || scl === "") { $("#sv_msg").textContent = "Enter both SDA and SCL GPIOs first."; return; }
  svPost("/api/servo/i2c?sda=" + sda + "&scl=" + scl);
});
$("#sv_slider").addEventListener("input", () => $("#sv_angle").textContent = $("#sv_slider").value + "°");
$("#sv_slider").addEventListener("change", () => svPost("/api/servo/write?deg=" + $("#sv_slider").value));
$("#sv_speed").addEventListener("input", () => $("#sv_speedv").textContent = svSpeedLabel(+$("#sv_speed").value));
$("#sv_speed").addEventListener("change", () => svPost("/api/servo/speed?dps=" + $("#sv_speed").value));
$$(".sv-deg").forEach(b => b.addEventListener("click", () => {
  $("#sv_slider").value = b.dataset.deg; svPost("/api/servo/write?deg=" + b.dataset.deg); }));
$("#sv_sweep").addEventListener("click", () => svPost("/api/servo/sweep"));
$("#sv_attach").addEventListener("click", () => svPost("/api/servo/attach"));
$("#sv_detach").addEventListener("click", () => svPost("/api/servo/detach"));
let svBusy = false, svTick = 0;   // poll fast while a slewed move/sweep is in flight
setInterval(() => {
  if ($("#page-actions").offsetParent === null) return;
  if (svBusy || ++svTick % 3 === 0) svRefresh();
}, 500);

// ---- Shutters (per-blind calibration) --------------------------------------
let shList = [], shSel = "", calStep = 5, shBusy = false, shTick = 0, chFilled = false;
let svPca = false;   // learned from /api/servo — on a PCA9685 build, drive the selected shutter's channel

const shCurrent = () => shList.find(s => s.id === shSel);
const usTxt = v => (v == null || v < 0) ? "– µs" : v + " µs";

function shRenderSelect() {
  const sel = $("#sh_sel");
  sel.innerHTML = shList.map(s => `<option value="${s.id}">${esc(s.name)}</option>`).join("");
  if (!shList.some(s => s.id === shSel)) shSel = shList.length ? shList[0].id : "";
  sel.value = shSel;
}
function shRenderDetail() {
  const s = shCurrent();
  $("#sh_empty").classList.toggle("hidden", shList.length > 0);
  $("#sh_detail").classList.toggle("hidden", !s);
  if (!chFilled) {   // populate the PCA9685 channel dropdown once
    $("#sh_ch").innerHTML = Array.from({ length: 16 }, (_, i) => `<option value="${i}">${i}</option>`).join("");
    chFilled = true;
  }
  if (!s) return;
  if (document.activeElement !== $("#sh_name")) $("#sh_name").value = s.name;
  $("#sh_ch").value = s.channel;
  $("#sh_invert").checked = !!s.inverted;
  $("#sh_slug").textContent = s.id;
  $("#sh_status").textContent = s.calibrated ? "● calibrated" : "not calibrated";
  $("#sh_status").style.color = s.calibrated ? "var(--green)" : "var(--amber)";
  // Slider ends: default left = CLOSED / right = OPEN; the invert option swaps them so the
  // labelled direction always matches the position scale (and flags an accidental reversal).
  const leftOpen = !!s.inverted;
  $("#cal_endL").textContent = leftOpen ? "OPEN" : "CLOSED";
  $("#cal_endR").textContent = leftOpen ? "CLOSED" : "OPEN";
  $("#cal_endLv").textContent = usTxt(leftOpen ? s.openUs : s.closedUs);
  $("#cal_endRv").textContent = usTxt(leftOpen ? s.closedUs : s.openUs);
  // Test positions — values + disable Go until a position is calibrated/saved.
  $("#pos_open").textContent = usTxt(s.openUs);
  $("#pos_close").textContent = usTxt(s.closedUs);
  $("#fav_dl").textContent = usTxt(s.daylightUs);
  $("#fav_pv").textContent = usTxt(s.privacyUs);
  $("#pos_open_go").disabled  = s.openUs   < 0;
  $("#pos_close_go").disabled = s.closedUs < 0;
  $("#fav_dl_go").disabled    = s.daylightUs < 0;
  $("#fav_pv_go").disabled    = s.privacyUs  < 0;
}
function calRender(v) {   // v = /api/servo status
  svPca = !!v.usesPca;
  $("#cal_us").textContent = v.us;
  const scrub = $("#cal_scrub");
  scrub.min = v.min; scrub.max = v.max;
  if (document.activeElement !== scrub) scrub.value = v.moving ? v.targetUs : v.us;
  $("#cal_state").textContent = v.attached ? (v.moving ? "moving…" : "holding") : "detached (released)";
  const s = shCurrent();
  if (s && s.calibrated && s.openUs !== s.closedUs) {
    let pct = Math.round(100 * (v.us - s.closedUs) / (s.openUs - s.closedUs));  // 0 % = closed
    if (s.inverted) pct = 100 - pct;                                           // invert option: 0 % = open
    $("#cal_pct").textContent = Math.max(0, Math.min(100, pct)) + " %";
  } else $("#cal_pct").textContent = "– %";
  if (document.activeElement !== $("#cal_speed")) {
    $("#cal_speed").value = Math.min(v.speed || 120, 120);
    $("#cal_speedv").textContent = (v.speed || 0) + " °/s";
  }
  shBusy = !!v.moving;
}
async function shLoad() {
  try { shList = await apiGet("/api/shutters"); shRenderSelect(); shRenderDetail(); } catch (e) {}
}
async function calRefresh() { try { calRender(await apiGet("/api/servo")); } catch (e) {} }
// Point the servo driver at the selected shutter's channel (PCA9685 builds only), so
// calibration/recall drive THAT blind — not whatever channel the Servo-test page left active.
// Switching channels moves nothing (each channel holds its own position); the returned status
// updates the calibration readout. On a direct-GPIO build there's one servo — just refresh.
async function shSyncChannel() {
  const s = shCurrent();
  if (svPca && s && s.channel >= 0) {
    try { calRender(await apiPost("/api/servo/channel?ch=" + s.channel)); return; } catch (e) {}
  }
  calRefresh();
}
async function shOnShow() { shLoad(); await calRefresh(); shSyncChannel(); }

// list-returning shutter mutations
async function shMutate(url, obj, keepSel) {
  try {
    shList = await apiPost(url, obj);
    if (!keepSel) shSel = shList.length ? shList[shList.length - 1].id : "";
    shRenderSelect(); shRenderDetail(); $("#sh_msg").textContent = "";
  } catch (e) { $("#sh_msg").textContent = "Failed: " + e.message; }
}
// servo-motion (returns servo status)
async function calPost(url) { try { calRender(await apiPost(url)); $("#cal_msg").textContent = ""; }
  catch (e) { $("#cal_msg").textContent = "Failed: " + e.message; } }

$("#sh_add").addEventListener("click", () => {
  const name = prompt("Name this shutter (e.g. Front room – left):", "Shutter " + (shList.length + 1));
  if (name === null) return;
  shMutate("/api/shutters/add", { name: name.trim(), channel: shList.length });
});
$("#sh_sel").addEventListener("change", () => { shSel = $("#sh_sel").value; shRenderDetail(); shSyncChannel(); });
const commitName = () => { const s = shCurrent(); const n = $("#sh_name").value.trim();
  if (s && n && n !== s.name) shMutate("/api/shutters/rename", { id: s.id, name: n }, true); };
$("#sh_name").addEventListener("change", commitName);
$("#sh_rename").addEventListener("click", commitName);
$("#sh_ch").addEventListener("change", async () => { const s = shCurrent();
  if (s) { await shMutate("/api/shutters/channel", { id: s.id, channel: $("#sh_ch").value }, true); shSyncChannel(); } });
$("#sh_invert").addEventListener("change", () => { const s = shCurrent();
  if (s) shMutate("/api/shutters/invert", { id: s.id, inverted: $("#sh_invert").checked }, true); });
$("#sh_del").addEventListener("click", () => { const s = shCurrent();
  if (s && confirm(`Delete "${s.name}" and its calibration?`)) shMutate("/api/shutters/remove", { id: s.id }); });

// transport + scrubber
$("#cal_runclose").addEventListener("click", () => calPost("/api/servo/run?dir=close"));
$("#cal_runopen").addEventListener("click", () => calPost("/api/servo/run?dir=open"));
$("#cal_stop").addEventListener("click", () => calPost("/api/servo/run?dir=stop"));
$("#cal_nudgeminus").addEventListener("click", () => calPost("/api/servo/jog?delta=" + (-calStep)));
$("#cal_nudgeplus").addEventListener("click", () => calPost("/api/servo/jog?delta=" + calStep));
$("#cal_scrub").addEventListener("input", () => $("#cal_us").textContent = $("#cal_scrub").value);
$("#cal_scrub").addEventListener("change", () => calPost("/api/servo/us?us=" + $("#cal_scrub").value));
$("#cal_speed").addEventListener("input", () => $("#cal_speedv").textContent = +$("#cal_speed").value + " °/s");
$("#cal_speed").addEventListener("change", () => apiPost("/api/servo/speed?dps=" + $("#cal_speed").value));
$("#cal_detach").addEventListener("click", () => calPost("/api/servo/detach"));
$$("#cal_step .seg-btn").forEach(b => b.addEventListener("click", () => {
  calStep = +b.dataset.step; $$("#cal_step .seg-btn").forEach(x => x.classList.toggle("active", x === b)); }));

// endpoint SET + favourites (need a selected shutter)
function needSel() { if (!shSel) { $("#cal_msg").textContent = "Add or select a shutter first."; return false; } return true; }
const setEdge = edge => needSel() && shMutate("/api/shutters/set-edge", { id: shSel, edge }, true);
$("#pos_open_set").addEventListener("click", () => setEdge("open"));
$("#pos_close_set").addEventListener("click", () => setEdge("closed"));
$("#fav_dl_save").addEventListener("click", () => needSel() && shMutate("/api/shutters/save-fav", { id: shSel, fav: "daylight" }, true));
$("#fav_pv_save").addEventListener("click", () => needSel() && shMutate("/api/shutters/save-fav", { id: shSel, fav: "privacy" }, true));
const recallGo = fav => needSel() &&
  apiPost("/api/shutters/recall", { id: shSel, fav }).then(calRender).catch(e => $("#cal_msg").textContent = e.message);
$("#pos_open_go").addEventListener("click", () => recallGo("open"));
$("#pos_close_go").addEventListener("click", () => recallGo("closed"));
$("#fav_dl_go").addEventListener("click", () => recallGo("daylight"));
$("#fav_pv_go").addEventListener("click", () => recallGo("privacy"));

setInterval(() => {   // live position poll while the page is visible
  if ($("#page-shutters").offsetParent === null) return;
  if (shBusy || ++shTick % 3 === 0) calRefresh();
}, 500);

// ---- OTA --------------------------------------------------------------------
async function loadOta() {
  try {
    const d = await apiGet("/api/info");
    $("#o_fw").textContent = "v" + d.fw; $("#o_chip").textContent = d.chip;
    $("#o_variant").textContent = d.variant || "—";
    $("#o_heap").textContent = fmtKB(d.free_heap);
    const lf = d.last_flash;
    $("#o_last").textContent = lf.type === "none" ? "none yet" : `${lf.type} — ${lf.ok ? "OK" : "FAILED"}`;
  } catch (e) {}
}
function otaLog(msg) {
  const l = $("#o_log"); if (l.querySelector(".empty")) l.textContent = "";
  const t = new Date().toLocaleTimeString();
  l.textContent += `[${t}] ${msg}\n`; l.scrollTop = l.scrollHeight;
}
function otaUpload(target, file) {
  return new Promise((res, rej) => {
    const x = new XMLHttpRequest(); x.open("POST", "/api/ota?target=" + target);
    x.upload.onprogress = e => { if (e.lengthComputable) $("#o_prog").value = Math.round(100 * e.loaded / e.total); };
    x.onload = () => { $("#o_prog").value = 0; let r = {}; try { r = JSON.parse(x.responseText); } catch (e) {}
      (x.status === 200 && r.ok) ? res() : rej(r.error || ("HTTP " + x.status)); };
    x.onerror = () => rej("network error");
    const fd = new FormData(); fd.append("file", file); x.send(fd);
  });
}
$("#o_fwup").addEventListener("click", async () => {
  const f = $("#o_fwfile").files[0]; if (!f) { otaLog("Choose a firmware .bin first."); return; }
  $("#o_fwup").disabled = true; otaLog("Uploading firmware " + f.name + "…");
  try { await otaUpload("firmware", f); otaLog("Firmware flashed — rebooting, reconnect in ~15s."); }
  catch (e) { otaLog("Firmware failed: " + e); } $("#o_fwup").disabled = false;
});
$("#o_fsup").addEventListener("click", async () => {
  const f = $("#o_fsfile").files[0]; if (!f) { otaLog("Choose a LittleFS .bin first."); return; }
  $("#o_fsup").disabled = true; otaLog("Uploading filesystem " + f.name + "…");
  try { await otaUpload("filesystem", f);
    otaLog("Filesystem flashed. Click Reboot to apply.");
    $("#o_reboot").classList.add("primary"); $("#o_reboot").classList.remove("ghost");
  } catch (e) { otaLog("Filesystem failed: " + e); } $("#o_fsup").disabled = false;
});
$("#o_reboot").addEventListener("click", async () => {
  if (!confirm("Reboot the hub now?")) return;
  otaLog("Rebooting — reconnect in ~15s…");
  try { await apiPost("/api/system/reboot"); } catch (e) {}
});

// ---- Logs -------------------------------------------------------------------
const RANK = { V: 0, D: 1, I: 2, W: 3, E: 4 };
let logBuf = [], logWs = null, chipOn = { E: 1, W: 1, I: 1, D: 1, V: 1 }, minLvl = "I", search = "";
const LOGCAP = 600;

function passes(l) {
  return chipOn[l.lvl] && RANK[l.lvl] >= RANK[minLvl] &&
    (!search || (l.tag + " " + l.msg).toLowerCase().includes(search));
}
function lineNode(l) {
  const secs = (l.t / 1000).toFixed(3).padStart(9, " ");
  const div = document.createElement("div"); div.className = "logline " + l.lvl;
  div.innerHTML = `<span class="t">[${secs}]</span><span class="lv">${l.lvl}</span>` +
    `<span class="tg">${esc(l.tag)}:</span><span class="mg">${esc(l.msg)}</span>`;
  return div;
}
const esc = s => s.replace(/[&<>]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;" }[c]));
function renderLogs() {
  const v = $("#lg_view"); v.innerHTML = "";
  const shown = logBuf.filter(passes);
  if (!shown.length) { v.innerHTML = '<div class="empty">No logs to display</div>'; }
  else shown.forEach(l => v.appendChild(lineNode(l)));
  $("#lg_count").textContent = logBuf.length;
  if ($("#lg_auto").checked) v.scrollTop = v.scrollHeight;
}
function addLine(l) {
  logBuf.push(l); if (logBuf.length > LOGCAP) logBuf.shift();
  if (!passes(l)) { $("#lg_count").textContent = logBuf.length; return; }
  const v = $("#lg_view"); const em = v.querySelector(".empty"); if (em) em.remove();
  v.appendChild(lineNode(l)); $("#lg_count").textContent = logBuf.length;
  if ($("#lg_auto").checked) v.scrollTop = v.scrollHeight;
}
function connectLogs() {
  if (logWs && (logWs.readyState === 0 || logWs.readyState === 1)) return;
  const proto = location.protocol === "https:" ? "wss" : "ws";
  logWs = new WebSocket(`${proto}://${location.host}/ws/logs`);
  $("#lg_ws").textContent = "connecting…";
  logWs.onopen = () => $("#lg_ws").textContent = "● live";
  logWs.onclose = () => { $("#lg_ws").textContent = "○ disconnected"; setTimeout(connectLogs, 3000); };
  logWs.onmessage = ev => {
    let d; try { d = JSON.parse(ev.data); } catch (e) { return; }
    if (Array.isArray(d)) { logBuf = d.slice(-LOGCAP); renderLogs(); } else addLine(d);
  };
}
function logsOnShow() { connectLogs(); renderLogs(); }
$$("#lg_chips .chip").forEach(c => c.addEventListener("click", () => {
  chipOn[c.dataset.lvl] = chipOn[c.dataset.lvl] ? 0 : 1; c.classList.toggle("on"); renderLogs(); }));
$("#lg_level").addEventListener("change", e => { minLvl = e.target.value; renderLogs(); });
$("#lg_search").addEventListener("input", e => { search = e.target.value.toLowerCase().trim(); renderLogs(); });
$("#lg_auto").addEventListener("change", renderLogs);
$("#lg_clear").addEventListener("click", () => { logBuf = []; renderLogs(); });
$("#lg_export").addEventListener("click", () => {
  const txt = logBuf.map(l => `[${(l.t / 1000).toFixed(3)}] ${l.lvl}/${l.tag}: ${l.msg}`).join("\n");
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([txt], { type: "text/plain" }));
  a.download = "shutter-hub-logs.txt"; a.click(); URL.revokeObjectURL(a.href);
});

// ---- Boot -------------------------------------------------------------------
connectLogs();                    // keep the log buffer warm regardless of page
go(location.hash.slice(1) || "info");
