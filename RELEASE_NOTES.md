# v0.0.3 — Web-UI refinement

Polishes the v0.0.2 skeleton's web interface. Still runs on a bare ESP32-D — no servo/PCA9685
hardware required.

## What's new since v0.0.2
- **Change network on its own page** — the System tab's WiFi section has a single **Change network**
  button that opens a dedicated `/wifi` page (scan → pick → password → **Set**, with a **Back** link).
- **Reset & Reboot at the bottom of every page** — both ask for confirmation first. **Reset** forgets
  WiFi and restarts into the setup portal; **Reboot** restarts the hub.
- **Three firmware actions** — **Flash firmware**, **Flash LittleFS**, **Flash both** (filesystem
  first, then firmware), instead of one auto-detecting button.
- **Removed** the management access-point toggle and the inline re-run-setup button (the "hub mode"
  idea). WiFi is changed in-browser; **Reset** stays as the portal fallback.

## Download (ESP32-D)
| First flash over USB (offset `0x0`) | Update over the air |
| ----------------------------------- | ------------------- |
| `shutter-hub-esp32d-full-v0.0.3.bin` | `shutter-hub-esp32d-ota-v0.0.3.bin` |

_ESP32-C3 bins are deferred to a later release._

## Flash it
First time, over USB:
```
esptool --chip esp32 write_flash 0x0 shutter-hub-esp32d-full-v0.0.3.bin
```
Then join `Shutter-Hub-Setup`, pick your network, and open `http://shutter-hub.local/`. After that,
update over the air from the **Firmware** tab (select the `-ota-` bin, **Flash firmware**). Saved WiFi
and settings live in NVS and survive updates.

## Build from source
PlatformIO project in [`firmware/`](firmware/) — `pio run` builds the ESP32-D target. See
[`firmware/README.md`](firmware/README.md). Full history in [CHANGELOG.md](CHANGELOG.md).
