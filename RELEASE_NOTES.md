# v0.2.1 — LittleFS web UI, live logs, MQTT/HA config + servo speed control

Rolls up everything since v0.1.0: the **v0.2.0 web-UI rebuild** (never published as its own
release) plus the **v0.2.1 servo speed control**. Still runs on a bare ESP32-D — no
servo/PCA9685/power hardware required.

## What's new since v0.1.0

### Web UI rebuild (v0.2.0)
- **LittleFS single-page app** — the interface moved out of embedded C++ strings into static
  assets served from LittleFS, with a persistent sidebar: **Info · MQTT · Actions · System ·
  OTA Update · Logs**. A tiny embedded recovery page still offers OTA if the filesystem image
  isn't flashed, so a device can't strand itself.
- **Live log stream** — every log line streams to the browser over WebSocket (`/ws/logs`):
  level filter chips (E/W/I/D/V), text search, auto-scroll, export, clear.
- **MQTT + Home Assistant** — real broker connection with configurable host/port/credentials/
  base-topic, availability (LWT), and **HA MQTT Discovery** for diagnostic sensors (WiFi signal,
  uptime) so the hub appears in Home Assistant. Per-shutter covers come with Phase 4.
- **System page** — Quick Actions (Reboot · Reset WiFi · Reset config), WiFi scan/connect,
  and an optional **web-authentication** (HTTP basic auth) toggle.
- **OTA page** — firmware + LittleFS uploaders with an upload log and reboot control.

### Servo speed control (v0.2.1)
- **Speed slider** on the Servo test tab — **5–120 °/s in steps of 5, default 25 °/s**,
  persisted in NVS. Angle commands, presets, and Sweep now **ramp** to the target at the set
  rate instead of snapping — slow enough to watch a real blind linkage move.
- Status shows *moving → N°* during a slewed move; **Detach / Stop sweep freeze the move where
  it is** — useful if a linkage binds mid-travel.
- REST: `POST /api/servo/speed?dps=N`; `GET /api/servo` gains `target`, `moving`, `speed`.

## Download (ESP32-D) — now three bins

| File | Use |
| ---- | --- |
| `shutter-hub-esp32d-full-v0.2.1.bin` | First flash over USB at offset `0x0` |
| `shutter-hub-esp32d-ota-v0.2.1.bin` | OTA page → **Upload Firmware** |
| `shutter-hub-esp32d-littlefs-v0.2.1.bin` | OTA page → **Upload LittleFS** (the web UI) |

_ESP32-C3 bins are deferred to a later release._

## Flash it

First time, over USB:
```
esptool --chip esp32 write_flash 0x0 shutter-hub-esp32d-full-v0.2.1.bin
```
Then join `Shutter-Hub-Setup`, pick your network, and open `http://shutter-hub.local/`.

**Upgrading from v0.1.0 or earlier over the air: flash BOTH images** — the `-ota-` firmware
*and* the `-littlefs-` filesystem (the web UI now lives in LittleFS). Upload the firmware first
(auto-reboots), then LittleFS, then reboot. Until the LittleFS image is flashed the device
serves a minimal recovery page with the two uploaders. Saved WiFi and settings live in NVS and
survive updates.

## Build from source

PlatformIO project in [`firmware/`](firmware/) — `pio run` builds the ESP32-D target and
`pio run -t buildfs` builds the filesystem image. See [`firmware/README.md`](firmware/README.md).
Full history in [CHANGELOG.md](CHANGELOG.md).
