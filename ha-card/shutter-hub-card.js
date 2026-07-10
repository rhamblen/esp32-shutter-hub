/*
 * shutter-hub-card — Home Assistant Lovelace card for the ESP32 Shutter Hub.
 *
 * Operating card only (Phase 4b, first cut): 1–6 named shutters side by side,
 * the four blind states (Open · Close · Daylight · Privacy) and a manual
 * position slider, applied to ALL shutters or to one selected shutter.
 *
 * Binds to the MQTT-discovered entities from ADR-0005:
 *   cover.<hub>_<id>                     open/close/stop/position
 *   button.<hub>_<id>_daylight|_privacy  recall the saved preset
 * The daylight/privacy button ids are derived from the cover id unless given.
 *
 * Solar heat protection (Phase 6, v0.6.0) is OPTIONAL — supply any of
 *   solar_switch: switch.<hub>_solar_automation
 *   solar_lux:    sensor.<hub>_light_level
 *   solar_state:  sensor.<hub>_solar_state
 * and the header grows a live lux caption plus an automation toggle. Omit them
 * and the card renders exactly as before, so no-sensor installs are unaffected.
 *
 * Calibration (raw µs, full-open/close endpoints, save-to-preset) is a
 * separate future card gated on firmware — see docs/ha-lovelace-card.md.
 * No build step: plain custom element, registered as a Lovelace resource.
 */
class ShutterHubCard extends HTMLElement {
  static getStubConfig(hass) {
    const ids = Object.keys((hass && hass.states) || {});
    const shutters = ids
      .filter((e) => e.startsWith("cover.") && e.includes("shutter_hub"))
      .sort()
      .map((entity) => ({ entity }));
    const pick = (p, m) => ids.find((e) => e.startsWith(p) && e.includes(m));
    const cfg = { title: "Shutters", shutters };
    const sw = pick("switch.", "solar_automation");
    const lux = pick("sensor.", "light_level");
    const state = pick("sensor.", "solar_state");
    if (sw) cfg.solar_switch = sw;
    if (lux) cfg.solar_lux = lux;
    if (state) cfg.solar_state = state;
    return cfg;
  }

  setConfig(config) {
    if (!config || !Array.isArray(config.shutters) || config.shutters.length < 1) {
      throw new Error('shutter-hub-card: set "shutters" to a list of 1–6 covers');
    }
    this._config = config;
    this._shutters = config.shutters.slice(0, 6).map((s) => {
      const entity = typeof s === "string" ? s : s.entity;
      if (!entity || !entity.startsWith("cover.")) {
        throw new Error('shutter-hub-card: each shutter needs a "cover." entity');
      }
      const base = entity.replace(/^cover\./, "button.");
      return {
        entity,
        name: (s && s.name) || null,
        daylight: (s && s.daylight) || base + "_daylight",
        privacy: (s && s.privacy) || base + "_privacy",
      };
    });
    // Solar is opt-in: any key absent simply hides that part of the header.
    this._solar = {
      sw: config.solar_switch || null,
      lux: config.solar_lux || null,
      state: config.solar_state || null,
    };
    this._selected = null; // null = all shutters
    this._dragging = false;
    this._built = false;
    if (this._card) this.removeChild(this._card), (this._card = null);
  }

  set hass(hass) {
    this._hass = hass;
    if (!this._built) this._build();
    this._update();
  }

  getCardSize() {
    return 3 + Math.ceil(this._shutters.length / 4);
  }

  _name(s) {
    if (s.name) return s.name;
    const st = this._hass && this._hass.states[s.entity];
    const fn = st && st.attributes && st.attributes.friendly_name;
    return (fn || s.entity).replace(/^shutter-hub\s+/i, "");
  }

  _pos(s) {
    const st = this._hass && this._hass.states[s.entity];
    if (!st || st.state === "unavailable" || st.state === "unknown") return null;
    const p = st.attributes && st.attributes.current_position;
    return typeof p === "number" ? Math.round(p) : st.state === "open" ? 100 : 0;
  }

  _targets() {
    return this._selected == null ? this._shutters : [this._shutters[this._selected]];
  }

  _preset(kind) {
    const h = this._hass;
    if (!h) return;
    this._targets().forEach((s) => {
      if (kind === "open") h.callService("cover", "open_cover", { entity_id: s.entity });
      else if (kind === "close") h.callService("cover", "close_cover", { entity_id: s.entity });
      else if (kind === "stop") h.callService("cover", "stop_cover", { entity_id: s.entity });
      else if (kind === "daylight") h.callService("button", "press", { entity_id: s.daylight });
      else if (kind === "privacy") h.callService("button", "press", { entity_id: s.privacy });
    });
  }

  _setPosition(pos) {
    const h = this._hass;
    if (!h) return;
    this._targets().forEach((s) =>
      h.callService("cover", "set_cover_position", { entity_id: s.entity, position: pos })
    );
  }

  // ---- Solar (optional) ----
  _solarSt(key) {
    const id = this._solar[key];
    return id && this._hass ? this._hass.states[id] : null;
  }

  _solarOn() {
    const st = this._solarSt("sw");
    return st ? st.state === "on" : null;
  }

  _solarLux() {
    const st = this._solarSt("lux");
    const v = st ? parseFloat(st.state) : NaN;
    return isNaN(v) ? null : Math.round(v);
  }

  _toggleSolar() {
    if (this._solar.sw && this._hass) {
      this._hass.callService("switch", "toggle", { entity_id: this._solar.sw });
    }
  }

  _glyph(pos, selected) {
    const open = (pos == null ? 0 : pos) / 100;
    const th = (6 - open * 4).toFixed(1); // slat thickness: 6px closed → 2px open
    const col = selected ? "var(--primary-color)" : "var(--secondary-text-color)";
    const bars = [8, 19, 30, 41]
      .map((y) => `<rect x="7" y="${y}" width="26" height="${th}" rx="1.5" fill="${col}"/>`)
      .join("");
    return (
      `<svg viewBox="0 0 40 54" width="34" height="46" aria-hidden="true">` +
      `<rect x="2" y="2" width="36" height="50" rx="4" fill="none" stroke="var(--divider-color)"/>` +
      bars +
      `</svg>`
    );
  }

  _build() {
    const card = document.createElement("ha-card");
    card.innerHTML = `
      <style>
        .sh-wrap { padding: 12px 14px 14px; }
        .sh-head { display:flex; align-items:center; gap:8px; margin-bottom:12px; font-size:16px; font-weight:500; }
        .sh-head ha-icon { color: var(--secondary-text-color); }
        .sh-sp { margin-left:auto; }
        .sh-solarwrap { display:flex; align-items:center; gap:8px; }
        .sh-lux { font-size:12px; font-weight:400; color:var(--secondary-text-color); }
        .sh-solar { display:flex; background:none; border:none; padding:2px; border-radius:6px;
          cursor:pointer; color:var(--secondary-text-color); }
        .sh-solar.on { color:var(--primary-color); }
        .sh-solar ha-icon { --mdc-icon-size:20px; color:inherit; }
        .sh-tiles { display:grid; grid-template-columns:repeat(auto-fit,minmax(72px,1fr)); gap:8px; margin-bottom:12px; }
        .sh-tile { display:flex; flex-direction:column; align-items:center; gap:4px; padding:8px 4px;
          background:var(--secondary-background-color); border:1px solid var(--divider-color);
          border-radius:10px; cursor:pointer; color:var(--primary-text-color); font:inherit; }
        .sh-tile.sel { border-color:var(--primary-color); box-shadow:0 0 0 1px var(--primary-color) inset; }
        .sh-tile.dim { opacity:.45; }
        .sh-tname { font-size:12.5px; font-weight:500; text-align:center; line-height:1.15; }
        .sh-tpos { font-size:11px; color:var(--secondary-text-color); }
        .sh-target { display:flex; align-items:center; justify-content:center; gap:8px;
          font-size:12px; color:var(--secondary-text-color); margin-bottom:6px; }
        .sh-target b { color:var(--primary-text-color); font-weight:500; }
        .sh-all { background:none; border:none; color:var(--primary-color); cursor:pointer; font:inherit; font-size:12px; padding:0; }
        .sh-slider { display:flex; align-items:center; gap:10px; margin:2px 0 12px; }
        .sh-slider input { flex:1; }
        .sh-pct { min-width:40px; text-align:right; font-size:13px; font-weight:500; }
        .sh-btns { display:grid; grid-template-columns:repeat(4,1fr); gap:8px; }
        .sh-btns.stop { grid-template-columns:repeat(5,1fr); margin-top:8px; }
        .sh-btn { display:flex; flex-direction:column; align-items:center; gap:3px; padding:9px 2px;
          background:var(--secondary-background-color); border:1px solid var(--divider-color);
          border-radius:10px; cursor:pointer; color:var(--primary-text-color); font:inherit; }
        .sh-btn:hover { border-color:var(--primary-color); }
        .sh-btn ha-icon { --mdc-icon-size:20px; }
        .sh-btn span { font-size:12px; }
      </style>
      <div class="sh-wrap">
        <div class="sh-head"><ha-icon icon="mdi:window-shutter"></ha-icon><span class="sh-title"></span>
          <span class="sh-sp"></span>
          <span class="sh-solarwrap" style="display:none">
            <span class="sh-lux"></span>
            <button class="sh-solar" aria-label="Toggle solar automation"><ha-icon icon="mdi:sun-thermometer"></ha-icon></button>
          </span>
        </div>
        <div class="sh-tiles"></div>
        <div class="sh-target"><span class="sh-tlabel"></span><button class="sh-all" style="display:none">Select all</button></div>
        <div class="sh-slider"><input type="range" min="0" max="100" step="1" value="0"><span class="sh-pct">0%</span></div>
        <div class="sh-btns">
          <button class="sh-btn" data-k="close"><ha-icon icon="mdi:arrow-down-bold"></ha-icon><span>Close</span></button>
          <button class="sh-btn" data-k="privacy"><ha-icon icon="mdi:eye-off"></ha-icon><span>Privacy</span></button>
          <button class="sh-btn" data-k="daylight"><ha-icon icon="mdi:weather-sunny"></ha-icon><span>Daylight</span></button>
          <button class="sh-btn" data-k="open"><ha-icon icon="mdi:arrow-up-bold"></ha-icon><span>Open</span></button>
        </div>
        <div class="sh-btns stop">
          <button class="sh-btn" data-k="stop" style="grid-column:span 5"><ha-icon icon="mdi:stop"></ha-icon><span>Stop</span></button>
        </div>
      </div>`;

    this._els = {
      title: card.querySelector(".sh-title"),
      tiles: card.querySelector(".sh-tiles"),
      tlabel: card.querySelector(".sh-tlabel"),
      all: card.querySelector(".sh-all"),
      slider: card.querySelector(".sh-slider input"),
      pct: card.querySelector(".sh-pct"),
      solarWrap: card.querySelector(".sh-solarwrap"),
      solarBtn: card.querySelector(".sh-solar"),
      lux: card.querySelector(".sh-lux"),
    };
    this._els.solarBtn.addEventListener("click", () => this._toggleSolar());

    card.querySelectorAll(".sh-btn").forEach((b) =>
      b.addEventListener("click", () => this._preset(b.dataset.k))
    );
    this._els.all.addEventListener("click", () => {
      this._selected = null;
      this._update();
    });
    this._els.slider.addEventListener("input", () => {
      this._dragging = true;
      this._els.pct.textContent = this._els.slider.value + "%";
    });
    this._els.slider.addEventListener("change", () => {
      this._setPosition(parseInt(this._els.slider.value, 10));
      this._dragging = false;
    });

    this.appendChild(card);
    this._card = card;
    this._built = true;
  }

  _update() {
    if (!this._built || !this._hass) return;
    this._els.title.textContent = this._config.title || "Shutters";
    this._updateSolar();

    const tiles = this._els.tiles;
    if (tiles.childElementCount !== this._shutters.length) tiles.innerHTML = "";
    this._shutters.forEach((s, i) => {
      let tile = tiles.children[i];
      if (!tile) {
        tile = document.createElement("button");
        tile.className = "sh-tile";
        tile.innerHTML = `<span class="sh-g"></span><span class="sh-tname"></span><span class="sh-tpos"></span>`;
        tile.addEventListener("click", () => {
          this._selected = this._selected === i ? null : i;
          this._update();
        });
        tiles.appendChild(tile);
      }
      const pos = this._pos(s);
      const sel = this._selected === i;
      tile.classList.toggle("sel", sel);
      tile.classList.toggle("dim", this._selected != null && !sel);
      tile.querySelector(".sh-g").innerHTML = this._glyph(pos, sel);
      tile.querySelector(".sh-tname").textContent = this._name(s);
      tile.querySelector(".sh-tpos").textContent =
        pos == null ? "–" : pos === 0 ? "Closed" : pos === 100 ? "Open" : pos + "%";
    });

    const one = this._selected != null;
    this._els.tlabel.innerHTML = one
      ? `Controls: <b>${this._name(this._shutters[this._selected])}</b> only`
      : `Controls: <b>All shutters</b>`;
    this._els.all.style.display = one ? "" : "none";

    if (!this._dragging) {
      const list = this._targets().map((s) => this._pos(s)).filter((p) => p != null);
      const avg = list.length ? Math.round(list.reduce((a, b) => a + b, 0) / list.length) : 0;
      this._els.slider.value = avg;
      this._els.pct.textContent = avg + "%";
    }
  }

  _updateSolar() {
    const hasSolar = !!(this._solar.sw || this._solar.lux);
    this._els.solarWrap.style.display = hasSolar ? "" : "none";
    if (!hasSolar) return;

    const lux = this._solarLux();
    this._els.lux.textContent = lux == null ? "" : lux.toLocaleString() + " lx";

    const on = this._solarOn();
    this._els.solarBtn.style.display = this._solar.sw ? "" : "none";
    this._els.solarBtn.classList.toggle("on", on === true);
    const st = this._solarSt("state");
    this._els.solarBtn.title =
      "Solar automation " + (on == null ? "—" : on ? "on" : "off") +
      (st && st.state ? " · " + st.state : "");
  }
}

// Version tracks the project release tag (single SemVer stream — see docs/ai-context.md).
// HA caches card JS hard, so this console banner is how you tell which card is actually loaded.
const CARD_VERSION = "0.6.1";

customElements.define("shutter-hub-card", ShutterHubCard);
window.customCards = window.customCards || [];
window.customCards.push({
  type: "shutter-hub-card",
  name: "Shutter Hub Card",
  description: "Group + per-shutter control (Open/Close/Daylight/Privacy + position) for the ESP32 Shutter Hub",
});
console.info(
  `%c shutter-hub-card %c v${CARD_VERSION} `,
  "color:#fff;background:#ec4899;font-weight:700;border-radius:3px 0 0 3px;padding:2px 6px",
  "color:#ec4899;background:#171a24;font-weight:700;border-radius:0 3px 3px 0;padding:2px 6px",
);
