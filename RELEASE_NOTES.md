# v0.2.2 — Shutters calibration page + microsecond servo control

Per-blind calibration lands (Phase 2, in progress): a dedicated **Shutters** page next to the
existing **Servo test** bench diagnostic, driven by a microsecond-native `ServoController`.

## What's new since v0.2.1

### Shutters page — per-blind calibration
- A new **Shutters** sidebar page, separate from the low-level **Servo test** diagnostic (the old
  *Actions* tab, renamed). Define shutters (friendly name, PCA9685 channel — declared but not yet
  wired), then calibrate each in the browser.
- **µs scrubber** plus a video-editor-style **transport cluster** — slow-run open/close, **Stop**,
  and frame-step **nudge** (Fine 5 µs / Coarse 25 µs) — so a slat can be landed precisely.
- A single **Positions** panel holds all four targets — **Full open, Full close, Daylight,
  Privacy** — each with matching **Save current** / **Go** (disabled until set) controls.
- Slider ends are labelled **OPEN / CLOSED** with their pulse widths, and **swap when Invert is
  on** — travel direction is always explicit and can't be reversed by accident.
- Position shown as pulse width and derived **% of travel**. A per-shutter **Invert position
  scale** toggle flips the readout to 0 % = open (default 0 % = closed, the Home Assistant
  standard).
- Calibration persists in its own NVS namespace — survives a filesystem OTA *and* a config reset.
- UI mockup: [docs/diagrams/calibration-page.svg](docs/diagrams/calibration-page.svg).

### Microsecond-native servo control
- `ServoController` now tracks position in pulse width (µs) — the servo's native unit — for finer
  calibration than whole degrees. The Servo-test page still drives in degrees (derived).
- New REST: `POST /api/servo/{us,jog,run}` and the `/api/shutters/*` family
  (`add`/`remove`/`rename`/`channel`/`invert`/`set-edge`/`save-fav`/`recall`).

### Fixed
- **Stale web UI after reflashing** — static assets are now served with `Cache-Control: no-cache`
  so the browser always revalidates, instead of potentially caching an old `app.js`/`index.html`
  indefinitely (LittleFS files carry an epoch `Last-Modified`). **Do one hard refresh** (Ctrl+F5)
  after flashing this version.

## Download (ESP32-D) — three bins

| File | Use |
| ---- | --- |
| `shutter-hub-esp32d-full-v0.2.2.bin` | First flash over USB at offset `0x0` |
| `shutter-hub-esp32d-ota-v0.2.2.bin` | OTA page → **Upload Firmware** |
| `shutter-hub-esp32d-littlefs-v0.2.2.bin` | OTA page → **Upload LittleFS** (the web UI) |

_ESP32-C3 bins are deferred to a later release._

## Flash it

First time, over USB:
```
esptool --chip esp32 write_flash 0x0 shutter-hub-esp32d-full-v0.2.2.bin
```
Then join `Shutter-Hub-Setup`, pick your network, and open `http://shutter-hub.local/`.

**Upgrading over the air: flash BOTH images** — the `-ota-` firmware (the µs servo API and
Shutters endpoints live here) *and* the `-littlefs-` filesystem (the Shutters page lives here).
Upload the firmware first (auto-reboots), then LittleFS, then reboot — and **hard-refresh the
browser once** (Ctrl+F5) to pick up the new UI cleanly. Saved WiFi, shutters, and settings live in
NVS and survive updates.

## Build from source

PlatformIO project in [`firmware/`](firmware/) — `pio run` builds the ESP32-D target and
`pio run -t buildfs` builds the filesystem image. See [`firmware/README.md`](firmware/README.md).
Full history in [CHANGELOG.md](CHANGELOG.md).
