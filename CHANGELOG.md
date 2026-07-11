# Changelog

All notable changes to this project are documented here. Format based on
[Keep a Changelog](https://keepachangelog.com); this project uses [SemVer](https://semver.org).
Phases map loosely to minor versions (Phase 1 → v0.1.0).

## [Unreleased]

### Added
- **[docs/tilt-linkage-design.html](docs/tilt-linkage-design.html)** — self-contained interactive
  design document for the tilt-rod mechanism: exact four-bar solver + animated simulator (enter
  your shutter measurements, scrub the servo, get collision/torque flags, switch mounting presets),
  MG90D mount dimensions, and the 3D-printed parts list (P1 servo cradle, P2 slat-hook crank,
  P3 optional 22 mm arm). Served live via **GitHub Pages** (enabled, `main /docs`):
  <https://rhamblen.github.io/esp32-shutter-hub/tilt-linkage-design.html>.
- **[docs/diagrams/linkage-geometry.svg](docs/diagrams/linkage-geometry.svg)** regenerated from the
  solver: three-pose (closed / mid / open) dimensioned drawing of the rear-mount linkage, replacing
  the superseded twist-rod sketch; embedded in the README again.

### Changed
- **Tilt-rod kinematics corrected** in `docs/project-brief.md` §1 and `cad/README.md`. The original
  10 mm horn : 20 mm arm sizing treated the rod as a venetian-style *twisting* wand (~8 mm travel);
  the plantation hidden tilt rod actually **translates on a 36 mm arc** (~36 mm up + ~36 mm out,
  ~51 mm point-to-point), so the linkage is a genuine four-bar.
  `docs/diagrams/linkage-geometry.svg` is marked superseded.
- **Baseline linkage design settled after measuring the frame at 28 mm thick** (rules out any
  mortised pocket; the tilt rod sits flush at closed so it cannot be clamped). Both layouts drive a
  printed **slat-hook crank** (26 mm pin) with the stock 20 mm horn: **A (recommended, hidden)** —
  cradle on the bottom rail's *rear* face, axis (+23, −47), 58 mm pushrod, phase −10°, closes rear
  slat edge up; **B (visible)** — room-side face, axis (−21, −41), 52 mm pushrod, phase −170°,
  closes rear edge down. Both: ~122° sweep → 90° slat travel, jam margin ≥10 mm, torque ratio
  ≤0.86 (≈2.6 kg·cm at the slats). Slat hook sits ≥10 mm inboard of the rod's plane so rod and
  pushrod never meet.

### Documentation
Documentation-only audit against the v0.6.2 firmware. **No code, no behaviour, no version change.**

- **HomeKit status corrected everywhere.** Phase 5 is **built but unpaired** — the bridge boots and
  advertises, but no controller has ever completed pairing, and the work is parked. `project-plan.md`
  previously claimed pairing was *verified*, contradicting the README, `installation.md` and
  `user-guide.md`. The plan's status table now shows Phase 5 as ◐ with its exit criteria unmet, and
  `user-guide.md` leads its Apple Home section with the caveat rather than burying it.
- **Phase 6 marked ◐, not ☑** — the solar logic is built and exercised through the simulate-lux
  slider, but the physical VEML7700 is still not wired. The plan's own exit criteria already said so.
- **Lovelace card solar options documented** in [ha-card/README.md](ha-card/README.md) and
  [docs/ha-lovelace-card.md](docs/ha-lovelace-card.md) §2a — the `solar_switch` / `solar_lux` /
  `solar_state` keys shipped in v0.6.0 but appeared in neither, though `installation.md` points users
  at the card README for "the full option list". Includes the lux-vs-brightness distinction: the card
  captions **raw lux** because the state machine trips on raw lux, while `sensor.<hub>_brightness`
  (`20 × log10(lux)`, one decade per 20 points) is a display-only gauge that compresses the
  30–60 k lx threshold band to 90–96 % and must never drive automation.
- **`firmware/README.md` brought up to v0.6.2** — was still headed *"What this version does
  (v0.4.0)"*, listed `HomeKit.cpp` and `LightSensor.cpp` as stubs, omitted `SolarLogic.cpp` and the
  Solar page entirely, and named the filesystem image per-variant when one `esp32d-littlefs` image is
  shared across variants.
- **`architecture.md` config store corrected** — it described "LittleFS JSON (config + web files) +
  Preferences (calibration values)". All settings live in NVS; LittleFS holds only web assets. That
  split is what lets a filesystem OTA leave every setting intact (ADR 0005).
- **`ai-context.md` refreshed** — file map gained `installation.md`, `user-guide.md`, `pinout.md`,
  `ha-lovelace-card.md`, `ha-card/` and `RELEASE_NOTES.md`; flash figure corrected to 91.3 %
  (`esp32d-pca9685`) from a stale 91.2 %; the HomeKit entry no longer implies pairing works; dropped
  a reference to "v0.4.4", a changelog entry that was never tagged or released.
- **Shutter count is 1–4, not "any number from 1 upward"** (README, ai-context) —
  `Shutters::MAX` is 4; Phase 8 lists raising it to 6 as a prerequisite.
- **`project-brief.md`** now lists all twelve ADRs as decisions of record, not just 0001–0004.
- **`project-plan.md`** records v0.6.1 (selectable sensor bus, ADR 0012) and v0.6.2 (Info hardware
  table, brightness sensor) under Phase 6, and drops a stale note calling Phase 4 hardware-unproven.
- **`.gitignore`** now covers `firmware/dist/` — `*.bin` caught the images but not the card `.js`.

### Release checklist / notes
- **Bins are per-variant from v0.3.0 on** (board × servo backend): a `full` (USB flash) and `ota`
  (firmware) bin **per variant** — `shutter-hub-<variant>-{full,ota}-vX.Y.Z.bin` — plus a **single
  shared `littlefs`** image per version (`shutter-hub-esp32d-littlefs-vX.Y.Z.bin`; the web UI adapts
  to the backend at runtime, so it's identical across variants). Build firmware with
  `pio run -e <variant>` and the FS with `pio run -e <variant> -t buildfs`. Only the active ESP32-D
  variants (`esp32d-direct`, `esp32d-pca9685`) are shipped day-to-day; the C3 variants are deferred.
  **Flash the LittleFS image alongside the firmware** or the device serves the embedded recovery
  page. See [firmware/README.md](firmware/README.md).

## [0.7.2] — 2026-07-10

### Fixed
- **Dashboard/HomeKit tab showed "not paired" when the device was paired.** `HomeKit::paired()`
  returned a transient flag set only by the pairing *event* callback, which doesn't fire at boot for
  pairings already stored in NVS — so after a reboot or an OTA update (pairing survives both) the
  status read false even though Apple Home was still connected. It now derives from the authoritative
  controller list (`controllers() > 0`). Web UI unchanged — firmware-only fix.

### Changed
- **`ServoController::validGpio()` is now chip-aware — closes an ESP32-C3 bricking footgun.** It was a
  hard-coded ESP32-D whitelist; on a C3 it accepted GPIO25/26/27/32/33 (which don't exist) and
  GPIO12–17 (the SPI flash — driving them **bricks boot**). It's now `#if CONFIG_IDF_TARGET_ESP32C3`
  and the C3 build offers only `{0–10, 18–21}`, refusing the flash pins for both the servo signal pin
  and the PCA9685 I²C pins. The C3 is still an untested engineering build (LEDC channel cap, unverified
  hardware), but a servo/I²C pin can no longer be set to a value that bricks the board.

## [0.7.1] — 2026-07-10

Tidy-up of the v0.7.0 HomeKit fix. **No behavioural change** — same fix, cleaned up. The `%m`
HomeSpan patch and the HomeKit health watchdog carry over unchanged.

### Removed
- **All temporary HomeKit investigation diagnostics.** The `[hs]` breadcrumbs threaded through
  HomeSpan's `checkConnect()` (and the `hsCrumb` hook) are gone from `firmware/patches/HomeSpan.cpp`
  and `HomeKit.cpp` — the patch is now the `%m` fix only. The one-shot `[dbg]` bridge summary and the
  `ping` hint are removed.

### Changed
- **Log levels — second pass (completes the v0.7.0 first pass).** Per-action lines moved INFO → DEBUG:
  MQTT cover commands + `rx`, servo attach/detach/test-focus/sweep, HomeKit per-move targets, and the
  solar state-machine's intermediate "counting to trip/clear" / "cancelled" transitions. INFO is now
  reserved for milestones (boot, connections, config/calibration changes, pairing events, the solar
  *action* taken). The HomeSpan status callback is kept as a clean **DEBUG** line (`HomeSpan state: …`).

### Kept from v0.7.0
- The vendored HomeSpan `%m` patch (`firmware/patches/`), the mDNS-on-main-thread init, the HomeKit
  HAP-stall **WARN** watchdog, and `autoPoll` on core 1.

## [0.7.0] — 2026-07-10

**HomeKit pairing works.** The bridge is now discoverable in Apple Home and changes sync both
ways (Home ↔ HA). This is the diagnostic/rollback baseline — it still carries the investigation's
debug logging; v0.7.1 tidies it up.

### Fixed
- **HomeKit was completely undiscoverable and unpairable — root cause found & fixed.** With the bridge
  enabled the device advertised no `_hap._tcp`, the HAP server on port 1201 never opened, and the Home
  app saw nothing. The real cause was **not** mDNS or task/core (both were chased as red herrings):
  HomeSpan 1.9.1's `Span::checkConnect()` validates its mDNS hostname with
  `sscanf(hostName,"%m[A-Za-z0-9-]",&d)`, and **newlib-nano (this arduino-esp32 toolchain) does not
  implement the `%m` allocating conversion**. So `d` was never set, `strlen(hostName)!=strlen(d)` was
  falsely true, and HomeSpan hit its `while(1)` *PROGRAM HALTED* — silently, on the poll task, before
  it ever advertised the service or opened 1201. (`shutter-hub` is a perfectly valid hostname; the
  self-check was broken.)
  - **Fix:** a vendored patch replaces that check with a nano-safe character loop. It lives in
    `firmware/patches/HomeSpan.cpp` and is re-applied to `.pio/libdeps` before every build by
    `patches/apply_patches.py` (`extra_scripts` in `platformio.ini`), so it survives library
    reinstalls and applies to all four variants. See `firmware/patches/README.md`.
- **mDNS now initialised on the main thread** (`WebUI::begin()`, both modes) — hostname + `_http._tcp`;
  HomeSpan adds `_hap._tcp` on top. This replaced the earlier "HomeSpan is sole mDNS owner" scheme,
  which was itself a misdiagnosis of the `%m` hang.

### Added
- **HomeKit health watchdog (WARN).** `HomeKit::loop()` now logs a `WARN` if HomeSpan's connect
  callback hasn't fired within 10 s of boot — i.e. the bridge is stalled, not discoverable, 1201 closed.
  Previously it failed silently.
- **HomeKit diagnostics (temporary, removed in v0.7.1).** HomeSpan `setStatusCallback` streamed to the
  web **Logs** page; a one-shot bridge summary (`running/paired/controllers/hapUp/host/freeHeap`); and
  `[hs]` breadcrumbs threaded through HomeSpan's `checkConnect()` that pinpointed the `%m` hang.

### Changed
- **Log levels — first pass.** MQTT publish (`tx …`) traffic moved to **VERBOSE** (a failed publish now
  logs **WARN**); MQTT `subscribed …` lines and the HomeKit `[dbg]` lines moved to **DEBUG**, so INFO
  reads as a clean operational narrative. (Full Servo/Solar reclassification lands in v0.7.1.)
- **HomeSpan poll task** now runs on **core 1** (`autoPoll(16384,1,1)`), changed from core 0 while
  diagnosing; core was not the cause, but core 1 is tested-good and left as-is.

### Notes
- No behavioural change to servos, MQTT/HA control, or the solar logic. All four variants
  (`esp32d-{direct,pca9685}`, `esp32c3-{direct,pca9685}`) build with the fix; the C3 bins remain
  untested engineering builds.

## [0.6.2] — 2026-07-10

**Check the wiring from the Info page.** A hardware table naming every device, its bus, its pins and
its channel — plus HomeKit status alongside MQTT. And a human-readable brightness percentage next to
the raw lux reading.

### Added
- **Hardware & wiring table** on the **Info** page — one row per physical device, so the pinout on
  screen can be read straight off against the board:
  - **PCA9685 builds** — the driver's I²C bus (and whether the VEML7700 is **sharing** it or it's
    **servos only**), its SDA/SCL GPIOs, address `0x40`, and whether it's driving or released.
    Beneath it, one indented row per shutter giving its **channel** and calibration state.
  - **Direct-GPIO builds** — the servo's signal GPIO and an explicit *"no channel — single servo"*;
    no channel rows are drawn, because there is nothing to check.
  - **VEML7700** — dedicated (`Wire1`) or shared with the PCA9685 (`Wire`), the SDA/SCL GPIOs
    actually in use (the PCA9685's, when shared), address `0x10`, and detected / not detected /
    disabled. A footnote calls out a one-controller chip, a sensor not ACKing, and the fact that the
    servo bus pins are set on the Servo-test page.
- **HomeKit status on the Info page** — `Disabled` · `Enabled — reboot to start` ·
  `Active — not paired` · `Paired — N controller(s)`, matching the System ▸ HomeKit tab.
- **Brightness sensor (0–100 %)** — a human-facing companion to the raw lux reading, published
  retained on `<base>/solar/brightness` and discovered in Home Assistant as **Brightness** (`%`,
  `mdi:brightness-percent`). Also shown on the web UI's Solar ▸ Status card and exposed on
  `/api/solar` as `sensor.brightness`.
  - **Logarithmic, one lux decade per 20 points** — `20 × log10(clamp(lux, 1, 100000))`. Anchors:
    1 lx → 0 %, 10 → 20, 100 → 40, 1 k → 60, 10 k → 80, 100 k → 100. A dark room reads 0 %, a lit
    room 40 %, a bright room 54 %, overcast daylight 80 %, full sun 100 %.
  - A **linear** percent of the 120 k lx full scale was rejected: it reads 0 % for every indoor
    level and only starts moving in direct sun.

### Notes
- **Brightness is display-only.** The solar state machine still trips on **raw lux**. The log curve
  deliberately compresses the 30–60 k lx band the thresholds live in (clear = 90 %, trip = 96 %), and
  it isn't cleanly invertible — never drive automation from it. Raw `lux` keeps publishing unchanged,
  which is also what Phase 8b's HA-history calibration reads.
- The existing solar entities are **unchanged**: the `Solar automation` switch and the two writable
  lux threshold `number`s remain published to Home Assistant, and the Lovelace card keeps its
  solar toggle.
- **The sensor's low end is coarse by design.** At gain 1/8 one count is ~1.84 lx (≈ 5 % on this
  curve), so a dark room reads 0 % and the first step is visible. That's the price of a full scale
  that reaches direct sunlight.
- **The Info page is a mirror, not a control.** Every value in the hardware table is set elsewhere —
  servo bus pins on **Servo test**, channels on **Shutters**, the sensor bus on **Solar**. It exists
  so all three can be checked against the board in one place.

### Changed
- `GET /api/info` gained four objects: `servo` (`usesPca`, `pin`, `channel`, `sda`, `scl`,
  `attached`), `sensor` (`enabled`, `present`, `bus`, `sda`, `scl`, `dedicatedSupported` — the pins
  are the **active** ones, after any clamp to the shared bus), `homekit` (`enabled`, `running`,
  `paired`, `controllers`), and `shutters` (`name`, `channel`, `calibrated` per blind).
- `ha-card/shutter-hub-card.js` — `CARD_VERSION` stamped `0.6.2` (rides the project SemVer stream;
  no functional change to the card).

## [0.6.1] — 2026-07-10

**The sensor's I²C bus is now a setting** — and with it, solar heat protection works on the ESP32-C3.
Plus the full installation and user guides, and a pinout reference.

### Added
- **Selectable sensor bus** ([ADR 0012](docs/decisions/0012-selectable-sensor-i2c-bus.md)) — the
  VEML7700 can sit on its **own `Wire1`** (default, SDA 25 / SCL 26 — the fault-isolated choice from
  ADR 0011) **or share the PCA9685's `Wire`** (GPIO21/22). Chosen on the **Solar** page. A dedicated
  bus needs a second I²C controller, so on a one-controller chip the preference is **clamped to
  shared** and the dedicated option is disabled with an explanation.
- **[docs/installation.md](docs/installation.md)** — end-to-end install guide: USB first flash,
  captive-portal WiFi setup, servo smoke test, per-shutter calibration, MQTT/Home Assistant,
  the Lovelace card, HomeKit pairing, solar thresholds, web auth, and OTA updates. Each step
  carries its own troubleshooting table.
- **[docs/user-guide.md](docs/user-guide.md)** — everyday operation: the four saved positions,
  the four control faces (web UI, HA, Apple Home, Lovelace card), solar behaviour incl. hysteresis
  and the 2 h manual override, recalibration, and routine maintenance. Embeds the Shutters and Solar
  page mockups, footnoted as mockups rather than captures.
- **[docs/pinout.md](docs/pinout.md)** — GPIO map for both boards: the ESP32-D defaults the firmware
  actually ships, the pins `validGpio()` rejects and why, and a **proposed** ESP32-C3 map.

### Fixed
- **Solar failed silently on the ESP32-C3.** The C3 has one I²C controller (`SOC_I2C_NUM == 1`) to
  the ESP32-D's two, but the Arduino core declares `Wire1` unconditionally — so the C3 build linked
  cleanly and then `Wire1.begin()` returned `ESP_ERR_INVALID_ARG` at runtime. The sensor reported
  *"not detected"* forever, with nothing to distinguish it from a wiring fault. The bus setting above
  fixes this: the C3 falls back to the shared bus and the sensor works.
- **GPIO34–39 could be saved as dedicated I²C pins.** They're input-only and can never drive an
  open-drain I²C line. `POST /api/solar` now validates dedicated-bus pins with
  `ServoController::validGpio()` (correct here — dedicated mode only exists on the ESP32-D) and
  rejects identical, non-output-capable, or strapping pins.
- **Stale "shared I2C bus" notes**, which contradicted [ADR 0011](docs/decisions/0011-dedicated-sensor-i2c-bus.md)
  and the shipped v0.6.0 firmware. `project-brief.md` claimed "PCA9685 and VEML7700 share one bus"
  and omitted the sensor bus from its pinout table; `architecture.md` still gave "shared I2C with
  sensor" as a reason to pick the PCA9685; `project-plan.md` D9 referred to "the shared I2C bus".

### Changed
- **README** — new **Installation** section (five-step summary linking to the guides above); the
  hardware table now carries default pins per component; new documents listed in the repo-layout table.
- `AppConfig::setLightSensor()` takes a `bus` argument; new `lsBus()` / `SensorBus` enum in NVS.
  `LightSensor` gained `dedicatedSupported()`, `activeBus()`, `activeSda()`, `activeScl()`, and
  `/api/solar` reports all four so the (variant-shared) web UI can adapt at runtime.

### Notes
- **`reconfigure()` never tears down a shared bus.** `TwoWire::end()` really does deinit the
  peripheral — calling it on `Wire` would kill the PCA9685 and freeze every servo. Only the dedicated
  `Wire1` is ever ended. Conversely `TwoWire::begin()` **is** idempotent on an already-started master
  bus, which is why shared mode needs no ownership handshake with `ServoController`.
- **Shared mode re-couples sensor and servos** — a shorted sensor lead can wedge the servo bus. It is
  opt-in, off by default on the ESP32-D, and the UI says so. ADR 0011's reasoning is unchanged; it is
  now the default rather than the only mode.
- **The ESP32-C3 is still not shippable**, and no C3 binaries are attached to releases. This release
  removes the *solar* blocker; `validGpio()` still carries the ESP32-D whitelist and would accept
  pins that don't exist on a C3 (25/26/27/32/33) or that are its SPI flash (12–17). See
  [docs/pinout.md](docs/pinout.md).
- Flash: **91.3 %** (`esp32d-pca9685`), 91.9 % (`esp32d-direct`); C3 envs link at 89.5 % / 90.1 %.

## [0.6.0] — 2026-07-10

**Phase 6 — light sensor + solar heat protection.** The hub now watches the sun and moves the
shutters before the room bakes. A **VEML7700** ambient-light sensor feeds a trip/clear state
machine with dwell timers; the whole feature is configurable from a new **Solar** page, exposed to
Home Assistant over MQTT, and surfaced on the Lovelace card. It can be built, driven and tested
**before the sensor physically exists** via a simulate-lux slider.

### Added
- **`LightSensor`** — real VEML7700 driver (was a stub) on its **own I²C bus, `Wire1`**, pins
  configurable (default **SDA 25 / SCL 26**). Lean vendored register driver, no new library
  dependencies. Fixed gain 1/8 + 25 ms integration → full scale ≈ 120 k lx.
- **`SolarLogic`** — `idle → counting-trip → tripped → counting-clear → idle` state machine.
  Trips when lux stays **above** the trip threshold for its dwell; clears when it stays **below**
  the clear threshold for its dwell. The gap between the two is the hysteresis that stops passing
  clouds flapping the shutters.
- **Five-option actions.** The bright and clear targets are each one of **Open · Closed ·
  Daylight · Privacy · Do nothing**. `Do nothing` advances the state but drives no servos — so the
  default clear leaves the slats where the trip put them, and setting *both* to `Do nothing` makes
  the hub a pure sensor/reporter.
- **Manual override.** A user move on a shutter (web recall, or an MQTT `set` / `position/set` /
  jog / recall from Home Assistant) suspends solar automation **on that shutter for 2 hours**. The
  hub-wide state still tracks the light; suspended shutters are simply skipped when an action fires.
- **Solar page** in the web UI — Status (live lux, state, countdown, **simulate-lux slider** +
  "Use live sensor"), Sensor (type, Wire1 pins, live `detected @0x10` pill, enable), Sensitivity
  (four numbers: trip lux/dwell, clear lux/dwell), Actions (the two target selectors + automation
  switch). New `/api/solar` (GET/POST) and `/api/solar/simulate` routes.
- **MQTT / Home Assistant** — new hub-wide entities via discovery: an **illuminance sensor**
  (`<base>/solar/lux`), a **solar state sensor**, a **`Solar automation` switch**, and two writable
  **`number`** entities for the trip and clear lux thresholds. The two numbers are the write-back
  hook a future HA-side calibration card will use. Threshold writes that would put clear ≥ trip are
  rejected (inverted hysteresis oscillates). Topics documented on the MQTT ▸ Topics tab.
- **`shutter-hub-card` v0.6.0** — optional solar toggle + live lux caption in the group header,
  driven by three **optional** config keys (`solar_switch`, `solar_lux`, `solar_state`). Omit them
  and the card renders exactly as before, so no-sensor installs are unaffected.

### Changed
- **The light sensor no longer shares the PCA9685 I²C bus** — see
  [ADR 0011](docs/decisions/0011-dedicated-sensor-i2c-bus.md), which supersedes the "one shared
  bus, no clash" line in `architecture.md` / `ai-context.md`. A shorted sensor lead can no longer
  wedge the servo driver.
- `AppConfig` gained the sensor (enable/type/SDA/SCL) and solar (enable, thresholds, dwells, both
  targets) settings; all persist in NVS and are covered by **Reset config**.

### Notes
- **Reported lux is approximate.** The driver reports the linear `counts × resolution`; the Vishay
  high-lux correction polynomial is deliberately skipped because it diverges across the 60–120 k lx
  band the sensor exists to watch. Thresholds are calibrated against what *this* sensor reports —
  which is exactly what the planned HA-history calibration does anyway. Rationale in ADR 0011.
- **Solar config applies without a reboot** (unlike HomeKit) — saving re-inits the bus in place.
- Flash use is now **91.2%** on `esp32d-pca9685`. The lean driver choice was load-bearing: the
  Adafruit VEML7700 library (plus BusIO + Unified Sensor) would very likely not have fit.

## [0.5.4] — 2026-07-09

**HomeKit discovery, take 2 — single mDNS owner.** The Home app still couldn't discover the bridge
after v0.5.2. Comparing against the proven-good **HomeKey-ESP32** (which also runs HomeSpan + a web
server + MQTT) showed the real difference: *its web server never touches mDNS — HomeSpan is the sole
owner.* Ours had two initialisers of the one shared ESPmDNS responder (WebUI's `_http` + HomeSpan's
`_hap`), which left `_hap._tcp` unannounced.

### Fixed
- **WebUI no longer calls `MDNS.begin()` when HomeKit is enabled** — it defers to HomeSpan, which
  becomes the sole mDNS owner (same hostname via `setHostNameSuffix("")`, so `<name>.local` still
  resolves; `HomeKit::begin()` re-adds the `_http` service under HomeSpan's responder). When HomeKit
  is disabled, WebUI owns mDNS exactly as before. Removed the v0.5.2 `MDNS.end()` band-aid (there's
  now nothing to tear down — only one initialiser runs).

### Notes
- Confirmed the network is fine: other HomeKit accessories pair normally on the same 5 GHz phone, so
  the fault was our advertisement, not the WiFi. This is the fix for "iPhone finds nothing".

## [0.5.3] — 2026-07-09

**HomeKit no longer freezes the servos.** With the bridge enabled, servo control (both the Servo-test
page and Home Assistant/MQTT) stopped working; disabling HomeKit restored it — confirmed by A/B test
against v0.4.1 and against v0.5.1 with HomeKit off.

### Fixed
- **`homeSpan.poll()` was starving the main loop.** It was called from the Arduino `loop()`, sharing
  the single cooperative loop with `ServoController::loop()` (which writes each slewed servo frame to
  the PCA9685) and `Mqtt::loop()`. HAP handling — especially the SRP pairing crypto — holds the CPU
  long enough that servo frames and MQTT commands were never serviced, so shutters didn't move and HA
  control went dead while the bridge ran. HomeSpan now runs on its **own FreeRTOS task**
  (`homeSpan.autoPoll(16384, 1, 0)` — 16 KB stack for the SRP math, core 0 alongside WiFi/AsyncTCP),
  leaving the main loop (core 1) free to slew servos and pump MQTT regardless of HAP activity.
  `HomeKit::loop()` no longer calls `poll()` — it only services the deferred pairing reset.

### Notes
- This is independent of the HomeKit **pairing/discovery** issue (that's network/mDNS — the iPhone
  must be on the same L2 network as the hub). v0.5.3 makes the hub *usable* with HomeKit enabled;
  discovery still requires the phone and hub to share a network segment.

## [0.5.2] — 2026-07-09

**HomeKit discovery fix.** On hardware the bridge ran and logged the right pairing code, but the
iPhone never found it in *Add Accessory* — so pairing (QR or manual) had nothing to attach to.

### Fixed
- **`_hap._tcp` mDNS service was never announced** because the web UI had already called
  `MDNS.begin()`; HomeSpan's own `MDNS.begin()` then no-ops on the already-running responder and the
  HAP service came out unannounced. `HomeKit::begin()` now calls `MDNS.end()` right before
  `homeSpan.begin()` so HomeSpan gets a clean mDNS init and advertises `_hap._tcp` properly; the web
  UI's `_http` service is re-added immediately after (so `<name>.local` keeps working). This is the
  fix for "iPhone finds nothing to pair with".

## [0.5.1] — 2026-07-09

**Reliable reboot.** Field-testing v0.5.0 exposed that the device often wouldn't restart on command
— OTA firmware flashes and the web *Reboot* buttons set a flag serviced by the Arduino `loop()`, and
when the loop is starved/blocked the restart never fired (so a freshly-flashed image never ran until a
physical power-cycle). Reboots now fire from a high-priority `esp_timer`, independent of `loop()`.
New firmware + filesystem bins.

### Fixed
- **`Diagnostics::scheduleReboot()`** — restarts via a one-shot `esp_timer` (dispatched on the
  high-priority timer task), so a reboot happens even if `loop()` is stalled. `/api/system/reboot`,
  *Reset WiFi*, and *Reset config* now send their response, do their NVS work inline, then schedule
  the restart — no more loop-serviced `pending*` flags.
- **OTA no longer auto-reboots**, removing a race: flashing firmware *then* filesystem could fire the
  restart mid-filesystem-write or lose it entirely. Flash firmware and/or filesystem in any order,
  then click **Reboot** (now reliable). The OTA page highlights *Reboot* after any flash.

### Changed
- `WiFiSetup::forget()` added (clear WiFi creds without restarting); `forgetAndReboot()` builds on it.

### Migration
- To get onto v0.5.1: OTA-flash the `…-ota-v0.5.1.bin` firmware (+ `…-littlefs-v0.5.1.bin`), then
  **power-cycle once** to activate it (v0.5.0's software reboot is the very thing being fixed, so it
  can't be relied on for this last hop). From v0.5.1 onward, the *Reboot* button is dependable.

## [0.5.0] — 2026-07-09

**Phase 5 — Apple HomeKit.** The HomeSpan bridge behind the config tab shipped in v0.4.4 is now
live: each configured shutter appears in Apple Home as a **Window Covering**, pairable directly
(no hub) and controllable alongside Home Assistant. New firmware **and** filesystem bins.

### Added
- **HomeSpan bridge** ([firmware/src/HomeKit.cpp](firmware/src/HomeKit.cpp)) — one HAP *Window
  Covering* accessory per shutter under a single bridge. `update()` drives the same
  `ServoController` slot as MQTT; `loop()` streams the live position back and, once the servo
  settles, syncs Apple Home's target to the real position so moves made elsewhere (MQTT, the web
  UI) are reflected in Home too. Position maths mirror the MQTT cover exactly (0 % = closed unless
  the shutter's scale is inverted); an uncalibrated shutter rejects target changes.
- **`homespan/HomeSpan @ ~1.9.1`** dependency — pinned to the 1.9.x line, the last that supports
  arduino-esp32 core 2.0.9 (HomeSpan 2.x requires core ≥ 3.3.0; upgrading the core would churn
  every other library, so it stays on 2.0.9).
- **Live bridge state** — `/api/homekit` now reports real `running`/`paired`/`controllers`; the
  System ▸ HomeKit tab shows *Running* / *Enabled — reboot to start* / *Disabled*, enables
  *Reset pairings* only when the bridge is up, and its pairing QR is now the real, scannable code.
- **Operable-by-default shutters (MVP)** — HomeKit is for *driving* blinds, not configuring them, so
  an uncalibrated shutter is now still operable in Apple Home: it falls back to the servo's pulse
  envelope (min = closed, max = open) for a usable 0–100 % range. Calibrate in the web UI for
  accurate travel; the accessory logs which mode it's in.
- **Active pairing code echoed to the web Logs page** at start-up (HomeSpan's own pairing log only
  reaches the USB serial console) — so the code the *running* bridge expects can be checked against
  what the tab shows.

### Fixed
- **Pairing failed after changing the setup code (“code not recognised” / QR hangs).** The pairing
  verifier is baked in at boot, so a code (or enable, or bridge-name) changed in the web UI never
  reached the running bridge until a restart — leaving the tab showing one code while the device
  paired against another. The HomeKit tab now has an explicit **Reboot to apply** button; *Save*
  no longer implies “applied”, and directs you to reboot before pairing.

### Changed
- **`HomeKit::loop()`** added to the main loop (pumps `homeSpan.poll()` + the deferred pairing
  reset). *Reset pairings* erases HomeKit pairing data and reboots (HomeSpan `'H'`), deferred ~400 ms
  so the HTTP response flushes first.
- HomeKit tab copy updated now the bridge is real (no longer "arrives with v0.5.0"); notes that
  enabling, or adding/removing shutters, takes effect after a reboot.

### Design decisions / coexistence
- **Port 1201 for HAP** (`setPortNum`) — the async web server keeps port 80.
- **mDNS hostname pinned to the device name** (`setHostNameSuffix("")`) so `<name>.local` still
  resolves the web UI; HomeSpan re-runs `MDNS.begin()`, so the `_http` service is re-asserted after.
- **WiFi stays with WiFiManager** — we're already connected before HomeSpan polls, so its
  `checkConnect()` sees `WL_CONNECTED` and never calls `WiFi.begin()` itself (no startup blip); it's
  handed the live creds only for its own later reconnects.
- **QR setup ID `SHUT`**, category *Bridges* (2), protocol IP (2) — the web UI's `X-HM://` payload
  is computed to match HomeSpan's own encoder byte-for-byte.
- **Flash is now ~90 %** of the 1.3 MB app partition (was ~63 %) — HomeSpan pulls in the HAP/SRP
  crypto. Still fits with OTA headroom, but Phase 6 (light sensor) must budget against the remaining
  ~10 %.

### Known limitations
- The accessory tree is built once at boot, so **any HomeKit change — enable, setup code, bridge
  name, adding/removing a shutter — needs a reboot** to take effect (MQTT still updates live). The
  tab now surfaces this with a *Reboot to apply* button.
- Pairing itself is confirmed on real hardware with an Apple device: save → *Reboot to apply* →
  scan the QR or enter the code shown. The active code is echoed to the web Logs page to cross-check.

## [0.4.4] — 2026-07-09

HomeKit **configuration tab** (Phase 5 groundwork) — the System ▸ HomeKit sub-tab now stores the
bridge settings, ahead of the HomeSpan bridge itself (v0.5.0). Same pattern as the MQTT tab, which
shipped its config before the Phase 4 integration. New firmware **and** filesystem bins.

### Added
- **System ▸ HomeKit sub-tab** — enable toggle, **bridge name** (defaults to the device name) and
  **setup code** (8 digits, shown as `XXX-XX-XXX`, with a *Random code* generator), bridge/pairing
  status rows, a disabled *Reset pairings* button (live once the bridge runs), and a **pairing QR
  code** (`X-HM://` payload, category *bridge*, setup ID `SHUT`) rendered client-side via
  `qrcode.min.js` (qrcodejs 1.0.0, MIT, added to the LittleFS bundle).
- **Default setup code `748-88-377`** — "SHUTTERS" on a phone keypad; deliberately distinct from
  HomeSpan's own default `466-37-726` ("HOMESPAN" by the same trick).
- **`/api/homekit` GET/POST + `/api/homekit/reset-pairings`** — config persisted to NVS
  (`AppConfig::hkEnabled/hkBridgeName/hkSetupCode`); the setup code is validated device-side
  against Apple's banned trivial codes (all-same digits, `12345678`, `87654321`). Reset-pairings
  returns 409 until the bridge exists.
- **[HomeKit.h](firmware/include/HomeKit.h) stub API** — `running()/paired()/controllers()/resetPairings()`,
  so Phase 5 only swaps the module's internals. Contract noted there: QR setup ID `SHUT`
  (`homeSpan.setQRID`), HAP off port 80 (`homeSpan.setPort(1201)`), WiFi stays with WiFiManager.
- **Icons on the tab bars** — inline `currentColor` SVGs on both sub-tab rows: System ▸ cog
  (Device), wifi arcs (WiFi), house (HomeKit), shield (Security); MQTT ▸ globe (Broker), inbox
  (Topics). On phones the icon'd tabs collapse to icon-only. (SSL/TLS was considered and
  deliberately left out — the broker is local-LAN Mosquitto, so plaintext MQTT is the norm and
  on-device TLS isn't worth the cert-management and handshake-RAM cost.)

### Changed
- Factory reset (`Reset config`) now also clears the HomeKit settings; confirm text updated.

## [0.4.3] — 2026-07-09

Card-only patch. Frontend only — **no firmware change**, so no new firmware/filesystem bins.

### Changed
- **`shutter-hub-card` button order** is now **Close · Privacy · Daylight · Open** (was
  Open · Close · Daylight · Privacy) — a closed→open progression. The deployed inline resource on
  *My Home › Shutters* was updated in place (hard-refresh to pick it up).

## [0.4.2] — 2026-07-09

Home Assistant **operating card** (Phase 4b, first cut) — a custom Lovelace card for the shutters.
Frontend only: **no firmware change**, so no new firmware/filesystem bins ship with this release.

### Added
- **`shutter-hub-card` — Lovelace operating tile** ([ha-card/shutter-hub-card.js](ha-card/shutter-hub-card.js)):
  1–6 named shutters side by side, each with a slat glyph that tracks position. The four blind states —
  **Open · Close · Daylight · Privacy** — plus a **manual position slider** (arbitrary 0–100 %) and a
  **Stop**, applied to **all shutters at once** or to **one selected shutter** (tap a tile to scope,
  *Select all* to return). Binds to the ADR-0005 discovery entities (`cover.<hub>_<id>` +
  `button.<hub>_<id>_daylight`/`_privacy`, the button ids auto-derived from the cover id). Plain custom
  element, no build step; themes off HA CSS variables. New [ADR-0007](docs/decisions/0007-ha-lovelace-card.md)
  and build spec [docs/ha-lovelace-card.md](docs/ha-lovelace-card.md).
- **Deployed to Home Assistant** — registered as an inline `module` dashboard resource and added as a
  new **Shutters** view on the **My Home** dashboard (both currently-configured shutters). Movable later.

### Notes
- **Calibration / config card is deferred**, on purpose: raw-µs set-and-go, the full-open/close travel
  endpoints, and *saving* into the four slots need firmware commands that don't exist yet (`goto_us`,
  raw-µs readback, `save:open`/`save:close`). Specified for a future phase in
  [docs/ha-lovelace-card.md](docs/ha-lovelace-card.md) §4; until then, calibrate in the hub web UI.
- The card caps at 6 shutters; the firmware still defines `Shutters::MAX = 4` — raising it to 6 is
  part of the deferred calibration-phase work.

## [0.4.1] — 2026-07-09

### Changed
- **Declared icons on the per-shutter HA buttons** — the MQTT discovery payloads now set each
  button's icon device-side, so the entities look right everywhere in HA (device page,
  auto-dashboards, pickers): **Daylight / Privacy recall** → `mdi:window-shutter-settings`,
  **Jog open / close** → `mdi:chevron-double-up` / `mdi:chevron-double-down`,
  **Save daylight / privacy** → `mdi:content-save-cog`. Retained configs republish on connect —
  entities update in place, nothing to redo in HA. The cover keeps its `device_class: shutter`
  state-driven icon.

## [0.4.0] — 2026-07-09

MQTT / Home Assistant cover control (Phase 4): every configured shutter becomes a native HA
`cover` plus six `button` entities via MQTT discovery, driving real servo channels — several at
once. Implements the [ADR-0005](docs/decisions/0005-mqtt-command-structure.md) command table
(the Phase-4b calibration-card commands — `goto_us`, `save:open`/`save:close`, raw-µs readback —
stay deferred to that phase).

### Added
- **Per-shutter MQTT cover control** — the hub subscribes to
  `<base>/cover/<id>/set` (`OPEN` | `CLOSE` | `STOP`), `<base>/cover/<id>/position/set` (`0–100`)
  and `<base>/cover/<id>/cmd` (`jog_open` | `jog_close` | `recall:daylight` | `recall:privacy` |
  `save:daylight` | `save:privacy`), wired to each shutter's calibrated endpoints/favourites and
  its own PCA9685 channel. Jog steps **25 µs** per press, clamped to the calibrated travel.
  `OPEN`/`CLOSE`/position commands on an **uncalibrated** shutter are ignored with a logged
  warning (jog and save still work).
- **Per-shutter state publishing** — `<base>/cover/<id>/position` (`0–100`) and
  `<base>/cover/<id>/state` (`opening|closing|open|closed|stopped`), both **retained**, published
  on change (≤4 Hz while moving). Position honours the per-shutter **Invert** scale; an
  uncalibrated shutter reports `0`/`closed` per the assembly-home convention
  ([ADR-0009](docs/decisions/0009-servo-position-memory.md)). Moves made from the web UI are
  reflected too — HA always tracks the real position.
- **Home Assistant discovery per shutter** — one `cover` (device class `shutter`, position +
  set-position + state + availability) and six `button` entities (Jog open/close,
  Daylight/Privacy recall, Save daylight/privacy — the save pair marked `entity_category:
  config`), all grouped under the hub device and sharing the hub's `<base>/status` LWT.
  Discovery **re-publishes automatically** when a shutter is added / removed / renamed /
  re-channelled / inverted / recalibrated — a deleted shutter's entities and retained state are
  removed from the broker (no reconnect needed).
- **Concurrent per-channel servo drive** ([ADR-0010](docs/decisions/0010-concurrent-servo-drive.md))
  — `ServoController` now keeps independent slew state per output ("slot"): every commanded
  channel moves simultaneously at the shared speed limit, so an HA "close all" really closes all.
  New slot API (`moveSlotUs`, `stopSlot`, `slotUs`, `slotTargetUs`, `slotMoving`); per-slot
  position memory in NVS (extends ADR-0009). Direct-GPIO builds are unchanged (one slot).
- **MQTT page — live Topics map** — the read-only Topics sub-tab now lists the real topics for
  every configured shutter (command / position-target / custom / position / state, with payload
  hints and direction badges), updating live as the base topic is edited, plus the hub-wide rows
  and a discovery-prefix note.

### Changed
- **Servo-test channel switching no longer freezes an in-flight move** — with per-slot state,
  changing the test focus lets the old channel finish its slew (previously it was abandoned
  mid-travel). Detach still releases only the active channel.
- **Shutters page channel hint** updated: on a PCA9685 build the per-shutter channel now routes
  MQTT/HA cover commands directly.

### Notes
- **Simultaneous peak current is now real** — ~3–4 A with four MG90Ds moving at once, which the
  XL4015 rail was sized for ([ADR-0003](docs/decisions/0003-power-chain-xl4015-pca9685.md)). On a
  USB-powered bench rig, avoid multi-shutter scenes. Soft-start staggering is deferred (ADR-0010).
- Uses wildcard subscriptions (`<base>/cover/+/…`), so no re-subscribe is needed when shutters
  change; discovery/state refresh is deferred to the MQTT task loop for thread safety.

## [0.3.0] — 2026-07-08

Build variants and a real PCA9685 servo backend. The servo hardware is now a compile-time choice,
identified everywhere so an OTA bin is never ambiguous.

### Added
- **Build variants — board × servo backend** — the servo driver is a compile-time choice,
  `-D USE_PCA9685=0|1`: **direct GPIO** (ESP32Servo, one bench actuator) or **PCA9685** (Adafruit PWM
  driver, I2C multi-channel — the production topology from [ADR-0003](docs/decisions/0003-power-chain-xl4015-pca9685.md)).
  Four PlatformIO envs — `esp32d-` / `esp32c3-` × `-direct` / `-pca9685` — each carrying an
  `FW_VARIANT` id; only the ESP32-D pair is active, **`esp32d-pca9685` is the default**, and the C3
  envs stay deferred. New [ADR-0008](docs/decisions/0008-build-variants.md).
- **Variant identity on-device** — `GET /api/info` gains `variant` (e.g. `esp32d-pca9685`) and
  `backend` (`gpio`|`pca9685`); both show on the **Dashboard** Device card and the **OTA** page, and
  name the release artifacts `shutter-hub-<variant>-{full,ota,littlefs}-vX.Y.Z.bin`.
- **PCA9685 servo backend** — on a `-pca9685` build, `ServoController` drives the servo through a
  PCA9685 at `0x40` over I2C (Adafruit PWM Servo Driver, 50 Hz, `writeMicroseconds`), releasing the
  channel (full-OFF) on detach. The µs position core (slew/speed/sweep) is shared with the direct
  build — only the hardware primitives switch.
- **Adaptive Servo-test page** — the one page renders the right controls for the running backend
  (from `/api/info`): the **direct** build keeps the signal-GPIO selector; the **PCA9685** build gets
  **I2C SDA/SCL pin selectors** and a **servo-channel (0–15) selector**. New config in `AppConfig`
  (`i2cSda`/`i2cScl` default 21/22, `servoChannel` default 0) and REST `POST /api/servo/channel`,
  `POST /api/servo/i2c`.

### Changed
- **`platformio.ini` restructured** into the four variant envs with backend mixins;
  `default_envs = esp32d-pca9685`. Added `adafruit/Adafruit PWM Servo Driver Library` to `lib_deps`
  (only linked on `-pca9685` builds).
- **Releases ship per-variant firmware** (`full` + `ota`) plus a **single shared LittleFS** image
  per version (the UI adapts at runtime, so the FS bin is variant-independent).
- `FW_VERSION` → **0.3.0**.

### Added
- **Servo position memory + "home" convention** ([ADR-0009](docs/decisions/0009-servo-position-memory.md)) —
  tackles the open-loop first-move jump (an unfed servo has no position, so its first pulse snaps the
  arm at full speed). `ServoController` now persists each drive slot's last position to NVS
  (`AppConfig::servoPos`/`setServoPos`, keys `svp0..svp15`; direct = slot 0, PCA9685 = channel) and
  restores it on boot, so a **warm reboot / OTA** (servos still powered) restores where the arm is and
  the first move slews from there — no snap. Writes are **debounced** (~3 s after a move settles, plus
  on detach and channel switch) to spare the flash. A never-driven channel defaults to **`HOME_US`**
  (the minimum-µs "closed" endpoint); fit the horn parked there at assembly (see
  [hardware-layout.md](docs/hardware-layout.md)) so a factory-fresh first boot also matches reality.

### Fixed
- **Channel switch no longer jolts the servo** — on a PCA9685 build, selecting a channel used to
  drive the *new* channel to the *previous* channel's pulse width at full speed (and release the old
  channel). Each channel now keeps its own remembered position, and switching focus **moves and
  releases nothing** — essential once a channel is a live blind. Actual moves still slew at the set
  speed.
- **Shutter calibration now drives the selected shutter's channel** — it was driving whichever
  channel the Servo-test page last left active, so every shutter appeared to move the one servo.
  Selecting a shutter (or reassigning its channel) now points the driver at that shutter's PCA9685
  channel first.

## [0.2.2] — 2026-07-08

Per-blind calibration (Phase 2): a dedicated Shutters page, microsecond-native servo control, and a
stale-web-UI cache fix.

### Added
- **Shutters page — per-blind calibration (Phase 2, in progress)** — a new **Shutters** sidebar page,
  separate from the low-level **Servo test** diagnostic (the old *Actions* tab, renamed). Define
  shutters (friendly name, PCA9685 channel — declared but not yet wired), then calibrate each in the
  browser: a **µs scrubber** plus a video-editor-style **transport cluster** (slow-run → **Stop** →
  frame-step **nudge**, Fine 5 µs / Coarse 25 µs). A single **Positions** panel holds all four targets —
  **Full open, Full close, Daylight, Privacy** — each with the same **Save current** (snapshot the live
  position) and **Go** (recall, disabled until set) controls, so open/close are managed exactly like the
  favourites. The **slider ends are labelled OPEN / CLOSED** with their pulse widths, and **swap when
  Invert is on**, so travel direction is always explicit and can't be reversed by accident. Position shown as
  both pulse width and derived **% of travel**. A per-shutter **Invert position scale** setup toggle
  flips the readout to **0 % = open** (default is **0 % = closed**, the Home Assistant standard). New
  `Shutters` module persists definitions + calibration in their **own NVS namespace** (survives a
  filesystem OTA *and* a config reset), per [ADR-0005](docs/decisions/0005-mqtt-command-structure.md).
- **UI mockup** — [docs/diagrams/calibration-page.svg](docs/diagrams/calibration-page.svg), a
  dark-theme wireframe of the Shutters page, linked from the Phase 2 plan.

### Fixed
- **Stale web UI after reflashing** — `serveStatic` on LittleFS sent no `Cache-Control`, and LittleFS
  files report an epoch `Last-Modified`, so browsers could cache `index.html`/`app.js`/`style.css`
  indefinitely. After flashing a new filesystem image the browser could keep running the *old* JS
  (old button wiring/ids) until a hard refresh — e.g. **Go** on Full open/close appearing to jump to
  the wrong saved position because stale JS was still bound to old element IDs. Static assets are now
  served with `Cache-Control: no-cache`, so the browser always revalidates. **If you've hit this,**
  do one hard refresh (Ctrl+F5 / disable cache in devtools) after flashing this version — every load
  after that self-corrects.

### Changed
- **`ServoController` is now microsecond-native** — position is tracked in pulse width (the servo's
  native unit) for finer calibration than whole degrees; the Servo-test page still drives in degrees
  (derived). New API: `writeUs`, `jogUs`, `run(dir)`, `minUs`/`maxUs`; `GET /api/servo` gains `us` and
  `targetUs`. New REST: `POST /api/servo/{us,jog,run}` and the `/api/shutters/*` family
  (`add`/`remove`/`rename`/`channel`/`set-edge`/`save-fav`/`recall`).
- `FW_VERSION` → **0.2.2**.

## [0.2.1] — 2026-07-08

Servo-test speed control, so bench moves can run slow enough for real blind linkages.

### Added
- **Servo speed control (Servo test tab)** — a **Speed** slider (5–120 °/s in steps of 5)
  limits how fast the bench servo slews. Angle commands and presets now **ramp** to the target at the
  set rate instead of snapping (`ServoController` keeps a current/target pair and steps it every 20 ms
  servo frame); Sweep obeys the same limit. The setting is **persisted**
  (`AppConfig::servoSpeedDps`, NVS, default **25 °/s**).
  New REST: `POST /api/servo/speed?dps=N` (0 = Max); `GET /api/servo` gains `target`, `moving`,
  `speed`. Status line shows *moving → N°* during a slewed move, and **Detach / Stop sweep freeze the
  move where it is** — a safety win when a linkage binds mid-travel.
- **[docs/hardware-layout.md](docs/hardware-layout.md)** — physical build plan for the hub
  prototype on a solderable copper breadboard: plan-view component placement (socketed ESP32,
  screw-terminal power spur, JST-XH I2C connectors), rail assignments with track cuts/bridges,
  nylon-standoff mounting + drill plan for the raised modules (PCA9685, XL4015, PD trigger,
  VEML7700), cable loom with connector recommendations, and a build-order checklist. Key rule:
  servo current runs point-to-point in 18 AWG and never through breadboard strips.

### Changed
- `FW_VERSION` → **0.2.1**. Servo test page polls at 0.5 s while a move/sweep is in flight
  (1.5 s when idle).

## [0.2.0] — 2026-07-07

Web-UI rebuild: the device interface becomes a LittleFS single-page app (sidebar shell modelled on
HomeKey-ESP32) with a live log stream and MQTT / Home Assistant scaffolding. Runs on a bare ESP32.

### Added
- **LittleFS single-page web UI** — the interface moved out of embedded C++ strings into static
  assets (`firmware/data/{index.html,style.css,app.js}`) served from LittleFS, with a persistent
  **sidebar**: **Info · MQTT · Actions · System · OTA Update · Logs**. The firmware now exposes a
  JSON/REST API + a WebSocket; a tiny **embedded recovery page** still offers OTA if the filesystem
  image hasn't been flashed, so a device can't strand itself.
- **Live log stream (Logs page)** — every `LOG*` line lands in an in-RAM ring buffer and streams to
  the browser over **`/ws/logs`** (WebSocket). Level filter chips (E/W/I/D/V), a minimum-level
  selector, text search, auto-scroll, export, and clear. Captures boot sequence, WiFi events, and
  MQTT rx/tx. New `LOGD` / `LOGV` macros; `Diagnostics` gains a log ring buffer + sink.
- **MQTT + Home Assistant scaffold** — real broker connection (`knolleary/PubSubClient`) with
  configurable host/port/client-ID/credentials/base-topic and a **HA MQTT Discovery** toggle, all
  persisted in NVS (`AppConfig::setMqtt`). Publishes hub availability (LWT) and **HA discovery for
  diagnostic sensors** (WiFi signal, uptime) so the device appears in Home Assistant. Per-shutter
  `cover` entities + command handling are Phase 4 (see below). `GET/POST /api/mqtt`.
- **System page** — a single **Quick Actions** card (Reboot · Reset WiFi · Reset config) plus
  **Device / WiFi / HomeKit / Security** sub-tabs. WiFi provisioning (scan/connect) moved here from
  the standalone `/wifi` page. **Security** adds a **web-authentication** toggle (HTTP basic auth,
  `AppConfig::setAuth`) guarding the UI + API; HTTPS shown as planned/disabled. **Reset config**
  factory-resets app settings while keeping WiFi credentials.
- **OTA page** — device-info strip, Firmware + LittleFS uploaders with an upload log, and a **Reboot**
  control (a firmware flash reboots automatically; a LittleFS flash needs a manual reboot to apply, so
  the button highlights after a filesystem upload).
- REST surface: `GET /api/info` (dashboard), `GET/POST /api/mqtt`, `GET/POST /api/auth`,
  `POST /api/system/{reboot,reset-wifi,reset-config}`; existing `/api/wifi/*`, `/api/servo/*`,
  `/api/ota` retained.

### Changed
- `FW_VERSION` → **0.2.0**. New deps: `PubSubClient` (+ `MQTT_MAX_PACKET_SIZE=1024`). LittleFS is
  mounted at boot (`LittleFS.begin(true)`); `Mqtt::loop()` is pumped from the main loop.
- **Reset/Reboot no longer sit in a per-tab footer** — they are consolidated into the System page's
  Quick Actions card (ADR-driven "one page only" decision).
- MQTT command structure + config ownership for Home Assistant locked in **ADR 0005** — browser-owned
  config, manual servo inventory, named **Daylight/Privacy** favourites, incremental jog for Up/Down.

### Migration
- **Flash the LittleFS image** (`-t buildfs` → `littlefs.bin`) with this firmware. Without it the
  device serves the recovery page (firmware/filesystem upload only).

## [0.1.0] — 2026-07-07

First hardware-phase release: single-servo bench test on top of the v0.0.3 skeleton.

### Added
- **Servo test tab (Phase 1)** — a new **Servo test** tab in the device web UI drives **one servo
  directly from an ESP32 GPIO** for hardware bring-up, before the PCA9685 / power chain exist. Signal
  pin defaults to **GPIO13** and is **changeable + persisted** (`AppConfig::servoPin`, NVS). Controls:
  set/validate the signal pin (rejects input-only/flash pins, flags strapping pins), a **0–180° angle
  slider**, **Min / Centre / Max** presets, a non-blocking **Sweep** toggle, and **Attach / Detach
  (release)**. Live status shows pin, attached/sweeping state, angle, and pulse width. The servo stays
  **detached at boot** so nothing moves until you act.
- **`ServoController`** promoted from stub to a real single-servo bench driver
  (`madhephaestus/ESP32Servo`, LEDC-backed; 50 Hz, 500–2500 µs). REST: `GET /api/servo`,
  `POST /api/servo/{pin,write,attach,detach,sweep}`.

### Changed
- `FW_VERSION` bumped to **0.1.0** (Phase 1 servo work).

## [0.0.3] — 2026-07-07

Web-UI refinement on top of v0.0.2.

### Added
- **Dedicated "Change network" page** (`/wifi`) — the WiFi section now has a single **Change network**
  button that opens its own page with the scan → select → password → **Set** flow and a **Back** link,
  instead of doing it inline.
- **Reset** and **Reboot** buttons pinned to the **bottom of the page** (below the tabs), each with a
  confirmation prompt. **Reset** forgets WiFi and restarts into the setup portal; **Reboot** restarts.
- **Three explicit firmware actions** on the Firmware tab — **Flash firmware**, **Flash LittleFS**, and
  **Flash both** (filesystem first, then firmware) — replacing the single auto-detecting button.

### Removed
- **Management access-point toggle** and the inline **Re-run setup** button (the "hub mode" idea) —
  along with the `apEnabled` setting, `/ap` route, `WiFiSetup::setSoftAP()`, and `ap_enabled` in
  `/info`. WiFi is changed in-browser; first-time setup still uses the portal, and **Reset** remains
  as the portal fallback.

## [0.0.2] — 2026-07-07

Skeleton completion on top of v0.0.1.

### Added
- **Tabbed device web UI** — three tabs: **System** — a **WiFi** section (network, IP, MAC, signal,
  access-point state, in-browser network picker + access-point toggle + Re-run setup) and a
  **System** section (device name,
  firmware version, hostname, uptime, boot count, free heap, last-restart reason, **Restart hub**,
  `/info`); **Firmware** (custom OTA, below); and **Apple Home** (blank placeholder for the Phase 5
  HomeKit UI).
- **Custom OTA page** (replaces ElegantOTA; mirrors HomeKey-ESP32's dual-target updater) — shows the
  installed firmware version, takes a **firmware** image and a **filesystem (LittleFS)** image in
  separate pickers, and flashes either independently or **both together** (filesystem first, then
  firmware, which reboots). Records the **last flash** — target, result, and UTC time (NTP) — shown
  on the page and in `/info`. Uploads stream through the core `Update` library; the ElegantOTA
  dependency is dropped.
- **`AppConfig`** — NVS/Preferences settings store (device name, boot count, AP-enabled flag) that
  survives OTA; the home for future shutter/HomeKit/MQTT settings.
- **`Diagnostics`** — structured serial logging (`LOGI/LOGW/LOGE`), a `/info` JSON endpoint
  (firmware, uptime, heap, reset reason, boot count, `ap_enabled`, WiFi), and a Reboot control.
- **In-browser WiFi change** — the WiFi section scans for nearby networks and lets you pick an SSID
  + password and **switch live, no reboot** (`GET /api/wifi/scan`, `POST /api/wifi/connect`). A wrong
  password reverts to the current network so you're never stranded. A **Re-run setup** fallback
  reboots into the setup portal for moving to a network you can't reach in-browser; first-time
  provisioning still uses the WiFiManager portal. (Replaces the earlier reboot-to-portal "Change
  WiFi" button.)
- **Management access point toggle** — the `Shutter-Hub-Setup` AP is explicitly torn down once WiFi
  is provisioned. A persisted setting (default **off**) plus a WiFi-section button can re-enable it
  as a management AP reachable at `192.168.4.1` — it serves the normal web UI and does **not**
  relaunch the setup portal.
- **Modular firmware layout** — `main.cpp` reduced to a thin entry point; real modules
  `AppConfig` / `Diagnostics` / `WiFiSetup` / `WebUI` / `Ota`, plus empty `begin()` stubs for
  `ServoController` (Phase 1/2), `Mqtt` (Phase 4), `HomeKit` (Phase 5), `LightSensor` (Phase 6).

### Changed
- Captive-portal responsiveness: disable WiFi modem power-save (`WiFi.setSleep(false)`), fixed AP
  channel, weak-AP filtering, and a "scanning takes a few seconds" note on the portal menu.
- Builds are ESP32-D only for now (`default_envs = esp32dev`); the ESP32-C3 target is deferred.

## [0.0.1] — 2026-07-07

First tagged release: documentation baseline + a buildable firmware framework with OTA.

### Added
- **Firmware framework scaffold (v0.0.1)** — first buildable ESP32 firmware (`firmware/`):
  PlatformIO project (Arduino Core) advertising `shutter-hub.local` over mDNS, serving a status
  page, and accepting browser OTA updates at `/update` (ElegantOTA + ESPAsyncWebServer). Runs on a
  bare ESP32 — no servo/PCA9685 hardware.
- **On-device WiFi provisioning** (WiFiManager) — first boot raises a `Shutter-Hub-Setup` access
  point with a captive portal; credentials are stored in NVS and **survive OTA updates**. No WiFi
  credentials are compiled into the binary (mirrors the HomeKey-ESP32 SoftAP + captive-portal
  pattern; pulled forward from Phase 3). A "Change WiFi" control clears saved creds and reopens the
  portal.
- Two board targets: `esp32dev` (ESP32-D, primary/locked board) and `esp32-c3-devkitm-1`
  (ESP32-C3). Each produces a merged full-flash image (one-time USB flash at `0x0`) and an
  app-only OTA bin; all four land in `firmware/dist/`. Releases ship both boards' bins + source.
- Master engineering brief consolidating all design discussion (`docs/project-brief.md`).
- Phased roadmap with status table and open decisions (`docs/project-plan.md`).
- Architecture notes and trade-off table (`docs/architecture.md`).
- Bill of materials, shutter facts, and power budget (`docs/inventory.md`).
- Cold-start map for future AI sessions (`docs/ai-context.md`).
- ADRs: 0001 hub vs independent nodes, 0002 servo = MG90D, 0003 power chain (XL4015) + PCA9685,
  0004 custom firmware vs ESPHome (with HomeKey-ESP32 reference).
- SVG diagrams: system architecture, power/wiring schematic, actuator linkage geometry
  (`docs/diagrams/`), embedded in the README.
- `hardware/` (KiCad schematic/PCB) and `cad/` (3D-printer source + STL/STEP) folders with READMEs.
- MIT `LICENSE` and `.gitignore`.

### Design decisions (locked)
- **Architecture:** central Shutter Hub + variable MG90D actuator modules (not 4 independent ESP32s).
- **Servo:** MG90D digital metal-gear micro servo.
- **Power:** USB-C PD → AITRIP trigger (12 V) → XL4015 @ 5.1 V (replaces LM2596), 1000–2200 µF cap.
- **Driver:** PCA9685 16-ch I2C, sharing the bus with a VEML7700 light sensor.
- **Linkage:** M2 × 50 mm ball-link pushrod; adjustable horn/arm ratios.
- **Firmware:** custom Arduino (HomeSpan + MQTT), not ESPHome.

### Notes
- OTA-first reordering: the web-server + ElegantOTA stack **and** the WiFiManager captive-portal
  provisioning were pulled forward from Phase 3 into a standalone "Phase S" scaffold, so every later
  phase flashes over WiFi instead of USB and no credentials ever live in the binary.
- No servo/sensor hardware built yet — the firmware runs on a bare ESP32 dev board.
