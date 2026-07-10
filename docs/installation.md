# Installation Guide — ESP32 Smart Shutter Hub

Repository: <https://github.com/rhamblen/esp32-shutter-hub>

Everything needed to go from a bare ESP32 to four calibrated shutters answering to Home Assistant,
Apple Home, and a browser. Follow the steps in order — each one assumes the previous one worked.

Day-to-day operation is covered separately in [user-guide.md](user-guide.md). The parts list is in
[inventory.md](inventory.md); the physical build is in [hardware-layout.md](hardware-layout.md).

---

## Prerequisites

| Requirement | Detail |
| ----------- | ------ |
| Hardware | ESP32-D DevKit, PCA9685, MG90D servos, VEML7700, 5.1 V supply — see [inventory.md](inventory.md) |
| Assembled | Servos wired to PCA9685 CH0–3, PCA9685 on GPIO21/22, VEML7700 on GPIO25/26 |
| WiFi | 2.4 GHz network (the ESP32 has no 5 GHz radio) |
| Flashing | A USB data cable, plus **either** [PlatformIO](https://platformio.org/) **or** [NodeMCU-PyFlasher](https://github.com/marcelstoer/nodemcu-pyflasher) / `esptool` |
| Home Assistant *(optional)* | A working install with the **MQTT integration** configured against a broker (e.g. Mosquitto add-on) |
| Apple Home *(optional)* | An iPhone/iPad and a HomeKit hub (HomePod or Apple TV) if you want remote access |

Home Assistant and Apple Home are both optional. The hub is fully usable from its own web UI alone.

---

## Step 1 — Get the firmware

Two routes. Downloading a release is the normal one; building from source is for when you have
changed the code.

### Option A — download a prebuilt release (recommended)

Go to the [Releases page](https://github.com/rhamblen/esp32-shutter-hub/releases) and download the
assets for the **`esp32d-pca9685`** variant (that is the real four-shutter hub; `esp32d-direct` is a
single-servo bench build). A release carries three files per variant:

| File | What it is | When you need it |
| ---- | ---------- | ---------------- |
| `shutter-hub-esp32d-pca9685-full-vX.Y.Z.bin` | Bootloader + partitions + app, merged | First USB flash only |
| `shutter-hub-esp32d-pca9685-ota-vX.Y.Z.bin` | App only | Every later update, over WiFi |
| `shutter-hub-esp32d-littlefs-vX.Y.Z.bin` | The web UI filesystem | With the first flash, and whenever the UI changes |

### Option B — build from source

```bash
git clone https://github.com/rhamblen/esp32-shutter-hub
cd esp32-shutter-hub/firmware
pio run -e esp32d-pca9685              # firmware  -> .pio/build/esp32d-pca9685/firmware.bin
pio run -e esp32d-pca9685 -t buildfs   # web UI    -> .pio/build/esp32d-pca9685/littlefs.bin
```

See [firmware/README.md](../firmware/README.md) for the full variant table and the `merge-bin`
recipe that produces the `-full-` image.

---

## Step 2 — First flash over USB

This is the **only** time you need a cable. Plug the ESP32 into USB.

**With PlatformIO** (source checkout):

```bash
pio run -e esp32d-pca9685 -t upload      # firmware
pio run -e esp32d-pca9685 -t uploadfs    # web UI filesystem — do not skip this
pio device monitor                       # optional: watch the boot log at 115200 baud
```

**With a downloaded release** (`esptool`):

```bash
esptool --chip esp32 write_flash 0x0 shutter-hub-esp32d-pca9685-full-vX.Y.Z.bin
esptool --chip esp32 write_flash 0x290000 shutter-hub-esp32d-littlefs-vX.Y.Z.bin
```

`0x290000` is where LittleFS lives in the ESP32 default partition table, which this firmware uses.
The filesystem image is the same for every variant — only the firmware differs.

In NodeMCU-PyFlasher, select the `-full-` bin and flash it to address `0x0`.

> **If you flash only the firmware and not the filesystem**, the hub boots into a plain embedded
> recovery page that offers nothing but an OTA uploader. That is by design — upload the LittleFS
> image from that page and the real UI appears. It is not a failed flash.

---

## Step 3 — Join it to your WiFi

Nothing is compiled into the binary, so the same image works on anybody's network.

1. On first boot the hub raises an access point called **`Shutter-Hub-Setup`**. Join it from a phone
   or laptop.
2. A captive portal opens automatically. If it does not, browse to `http://192.168.4.1`.
3. Pick your 2.4 GHz network, enter the password, save.
4. The hub reboots and joins your network. Credentials are written to **NVS**, a flash partition that
   OTA updates never touch — you do this once, ever.

Now open **`http://shutter-hub.local`**. If mDNS does not resolve on your machine (common on some
Windows and Android setups), find the hub's IP in your router's DHCP table and use that instead.

You should land on the **Dashboard**, showing firmware version, chip, variant, IP, and uptime.

---

## Step 4 — Prove the servos move

Before calibrating anything, confirm the power chain and the PCA9685 are alive. Go to
**Servo test**, pick channel 0, and sweep the angle slider. The servo should track it smoothly.

If nothing moves, the fault is almost always in this list:

| Symptom | Likely cause | Fix |
| ------- | ------------ | --- |
| No movement, no twitch | PCA9685 V+ not powered | The 5.1 V servo rail is separate from the ESP32's 3.3 V logic — both must be live |
| ESP32 reboots when the servo moves | Brown-out from inrush | Add the bulk capacitor across PCA9685 V+ / GND |
| Servo buzzes, drifts, never settles | No common ground | Tie the buck converter's ground to the ESP32's ground |
| Info page shows `direct` not `pca9685` | Wrong build variant flashed | Reflash the `esp32d-pca9685` image |

---

## Step 5 — Define and calibrate each shutter

This is the step that takes real time, and the one nothing else works without. Everything saves
instantly to NVS.

Open **Shutters** and, for each blind:

1. **Add shutter.** Give it a friendly name ("Front room – left") and assign its **PCA9685 channel**
   (0–3). The name generates a slug used as the MQTT id — rename freely, but note that renaming
   changes the entity ids in Home Assistant.
2. **Find fully closed.** Use **slow close** (`◀◀`) to run the servo down, **stop** (`⏸`) near the
   endpoint, then **nudge** (`⏮`/`⏭`) in 5 µs steps until the slats are just closed without the servo
   straining. Press **Save current** on the *Full close* row.
3. **Find fully open.** Same, in the other direction. **Save current** on *Full open*.
4. **Set the favourites.** Jog to a comfortable daytime angle → **Save current** on *Daylight*. Jog to
   a slats-closed-but-not-jammed privacy angle → **Save current** on *Privacy*.
5. **Test all four.** Press **Go** on each row and watch it land where you expect.

> **Never drive a servo past mechanical stall.** If a servo hums and stays hot at an endpoint, it is
> pushing against the linkage. Back the endpoint off by 25 µs and re-save. Stalled MG90Ds burn out.

**Invert position scale** is on the shutter's detail card. Leave it off unless the mechanics run
backwards: the Home Assistant convention is 0 % = closed, 100 % = open, and the card and HomeKit both
assume it.

Repeat for all four blinds. Positions survive reboots and OTA updates, so the first move after a
power cut slews rather than snapping.

---

## Step 6 — Connect Home Assistant (optional)

The hub speaks MQTT and publishes Home Assistant discovery messages, so there is no custom
integration to install and no YAML to write.

1. In Home Assistant, make sure the **MQTT integration** is set up against your broker. Note the
   broker's IP, port, and a username/password.
2. In the hub's web UI, open **MQTT → Broker** and fill in:

   | Field | Value |
   | ----- | ----- |
   | Broker Address | Your broker's IP, e.g. `192.168.1.60` |
   | Port | `1883` |
   | Client ID | Leave blank for `shutter-hub-<mac>` |
   | Username / Password | Your broker credentials |
   | Base Topic | `shutter-hub` |
   | Home Assistant MQTT Discovery | **on** |
   | Enable MQTT | **on** |

3. **Save & Apply changes.** The status pill turns green when the broker connection is up.
4. In Home Assistant, look under **Settings → Devices & Services → MQTT**. One device appears, with:

   | Entity | Role |
   | ------ | ---- |
   | `cover.*` (one per shutter) | Open / close / stop, position 0–100 % |
   | `button.*_daylight`, `button.*_privacy` | Recall the saved presets |
   | `button.*_jog_open`, `button.*_jog_close` | Nudge 25 µs |
   | `button.*_save_daylight`, `button.*_save_privacy` | Overwrite a preset with the current position (diagnostic) |
   | `sensor.*_light_level` | Lux from the VEML7700 |
   | `sensor.*_solar_state` | Solar state machine: idle / armed / tripped |
   | `switch.*_solar_automation` | Enable/disable solar heat protection |
   | `number.*_trip_lux`, `number.*_clear_lux` | The two thresholds, writable from HA |

The **MQTT → Topics** tab shows the live topic map if you want to drive the hub from something other
than Home Assistant. Commands go to `shutter-hub/cover/<id>/set` and
`shutter-hub/cover/<id>/position/set`; retained state comes back on `.../state` and `.../position`.

**Troubleshooting**

| Symptom | Likely cause | Fix |
| ------- | ------------ | --- |
| Pill stays red | Bad credentials, or broker not reachable | Check the **Logs** page — the MQTT error is logged verbatim |
| Connects, but no entities in HA | Discovery off, or HA's discovery prefix changed | Turn on **Home Assistant MQTT Discovery**; HA's prefix must be the default `homeassistant` |
| A deleted shutter still shows in HA | Retained discovery config on the broker | Deleting a shutter clears it; if it lingers, delete the device in HA |
| Covers show as unavailable | Hub offline, or LWT fired | Availability is `shutter-hub/status`; check the hub is powered and on WiFi |

---

## Step 7 — Install the Lovelace card (optional)

The [`ha-card/`](../ha-card/) directory holds `shutter-hub-card.js`, a single plain custom element —
no build step, no HACS requirement.

1. Copy `shutter-hub-card.js` to `/config/www/shutter-hub-card.js` on your Home Assistant machine.
2. **Settings → Dashboards → ⋮ → Resources → Add resource**, URL `/local/shutter-hub-card.js`, type
   **JavaScript Module**.
3. Hard-refresh the browser (Ctrl-Shift-R).
4. Add a manual card to a dashboard:

```yaml
type: custom:shutter-hub-card
title: Shutters
shutters:
  - entity: cover.shutter_hub_front_room_left
    name: Left
  - entity: cover.shutter_hub_front_room_left_centre
    name: Left Centre
  - entity: cover.shutter_hub_front_room_right_centre
    name: Right Centre
  - entity: cover.shutter_hub_front_room_right
    name: Right
```

Use **Developer Tools → States** to get your real `cover.*` entity ids — they follow your device and
shutter names. The card derives the Daylight/Privacy button entities from each cover id; if your
naming differs, set `daylight:` and `privacy:` explicitly per shutter. See
[../ha-card/README.md](../ha-card/README.md) for the full option list.

---

## Step 8 — Pair with Apple Home (optional)

The hub runs a [HomeSpan](https://github.com/HomeSpan/HomeSpan) bridge: each shutter appears in the
Home app as its own **Window Covering**, alongside — not instead of — the Home Assistant entities.

> **Status:** the bridge runs correctly (servos and MQTT keep working with it enabled), but device
> **discovery and pairing on the author's own network remain unresolved** and the work is parked. If
> the Home app does not find the bridge, that is the known open issue, not a mistake on your part.
> See [CHANGELOG.md](../CHANGELOG.md).

1. **System → HomeKit.** Set a **Bridge name** and an 8-digit **Setup code** (default `748-88-377`;
   **⟳ Random code** generates a fresh one).
2. **Save changes**, then **↻ Reboot to apply**. HomeKit changes — enabling it, the code, the name,
   adding or removing shutters — take effect *only* after a reboot, because the pairing verifier is
   computed at boot.
3. After the reboot, the page shows the bridge status and a **pairing QR code**.
4. In the Home app: **+ → Add Accessory → More options…**, or scan the QR with the iPhone camera.
5. All shutters appear at once as one bridge. Assign them to rooms.

If you add a shutter later, reboot the hub so HomeSpan republishes its accessory list.

---

## Step 9 — Set up solar heat protection (optional)

The VEML7700 sits on its **own I²C bus** (`Wire1`, GPIO25/26 by default) so a fault on the sensor
lead cannot take down the PCA9685 servo bus — see
[ADR 0011](decisions/0011-dedicated-sensor-i2c-bus.md).

> **Status:** built and compiling, **not yet verified against physical sensor hardware.** The
> simulate-lux slider works regardless, so you can exercise the whole state machine before the sensor
> is wired.

Open the **Solar** page.

1. **Sensor card.** Confirm the SDA/SCL GPIOs match your wiring (defaults 25 / 26 — avoid GPIO34–39,
   which are input-only and cannot drive I²C). Toggle **Enable sensor**. The detection pill should
   go green and the **Status** card should start reporting lux.
2. **Sensitivity card.** Set the thresholds:

   | Field | Meaning | Sensible start |
   | ----- | ------- | -------------- |
   | Trip when lux above | Direct-sun level for your window | `60000` |
   | …held for (minutes) | Dwell before acting | `10` |
   | Clear when lux below | Sun has moved off | `30000` |
   | …held for (minutes) | Dwell before releasing | `20` |

   The gap between trip and clear is **hysteresis** — it is what stops a passing cloud from flapping
   the shutters. Keep *clear* comfortably below *trip*.

3. **Actions card.** Choose where the shutters go **when bright** (typically `Privacy` or `Closed`)
   and **when clear**. Any action can be **Do nothing** — a "do nothing" clear leaves the slats where
   the trip put them, and setting both to **Do nothing** turns the hub into a pure light monitor that
   still reports to Home Assistant.
4. Toggle **Solar automation** on and **Save & Apply changes**.
5. **Test it without waiting for the sun.** Drag **Simulate lux** above your trip threshold, press
   **Use simulated**, and watch the state advance through *armed* to *tripped* — the dwell timer is
   real, so give it the configured minutes. Press **Use live sensor** when you are done.

**Manual override:** moving a shutter by hand — from the web UI, Home Assistant, or Apple Home —
suspends solar automation *on that shutter only* for **2 hours**. The rest keep automating. This is
deliberate: if you overrode the automation, you meant it.

Calibrate the thresholds against your own window over a few sunny days. Lux through glass varies
enormously with orientation, glazing, and season.

---

## Step 10 — Lock it down

**System → Security.** Turn on **Enable Web Authentication** and set a username and password
(defaults to `admin`). The hub is on your LAN with no transport encryption — HTTPS on-device is
listed as planned and is not available — so treat the web UI as trusted-network-only. Do not expose
`shutter-hub.local` to the internet.

---

## Updating later (OTA)

After the first USB flash, every update goes over WiFi from the **OTA Update** page.

1. Download (or build) the new `-ota-` firmware bin, and the `-littlefs-` bin if the release notes
   say the web UI changed.
2. **If both changed, upload the filesystem first** — the firmware upload reboots the board.
3. **Upload Firmware** with the app bin. The page shows a live upload log, then the hub reboots.

Your WiFi credentials, MQTT settings, shutter definitions, calibration, favourites, HomeKit pairings,
and solar thresholds all live in NVS and are untouched by either upload.

**Recovering a bad flash:** if the hub is unreachable, hold it in reach of a USB cable and reflash the
`-full-` image at `0x0`. NVS survives that too. To wipe settings deliberately, use **System → Quick
Actions → Reset config**, or **Reset WiFi** to drop back into the `Shutter-Hub-Setup` portal.

---

## Troubleshooting index

| Symptom | Where to look |
| ------- | ------------- |
| Web UI is a bare recovery page | LittleFS image never flashed — Step 2 |
| `shutter-hub.local` does not resolve | mDNS; use the IP from your router — Step 3 |
| Servos do not move | Power chain and common ground — Step 4 |
| A servo hums and gets hot | Endpoint driven past stall — Step 5 |
| Shutter opens when HA says close | **Invert position scale** — Step 5 |
| No entities in Home Assistant | Discovery toggle and broker credentials — Step 6 |
| Apple Home cannot find the bridge | Known open issue — Step 8 |
| Solar never trips | Thresholds, or automation switch off — Step 9 |
| Anything else | **Logs** page — a live WebSocket stream of everything the firmware logs |
