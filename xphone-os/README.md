# Lume firmware module (`xphone-os`)

> **Canonical fork state:** read [`../docs/lume/START-HERE.md`](../docs/lume/START-HERE.md)
> and [`../docs/lume/CURRENT-STATE.md`](../docs/lume/CURRENT-STATE.md). The
> detailed historical notes below describe the inherited upstream architecture;
> where they conflict, the Lume handoff wins.

Lume targets the **Xteink X3 only** (ESP32-C3, UC8253 792×528). Two
PlatformIO environments build the same X3 firmware with one compile-time
language each: `lume-x3-it` (default) and `lume-x3-en`. A pre-display I²C
fingerprint guard halts on non-X3 hardware. The reader and file-transfer server
derive from CrossPoint (MIT), and hardware/display support comes from the
vendored FreeInk SDK.

## Current Lume state (2026-08-14)

Firmware `0.1.0-dev`: X3-only, dedicated `Lume X3` BLE identity, Lume on-glass
identity, complete Italian/English compile-time localization, and the X3's
DS3231 driven as a real clock. The native iOS companion in `../ios/` implements
Priorities and Today/EventKit and has been verified on the physical iPhone/X3
path.

**Product — six focus apps, 3×2 launcher:**
* **Block** — triggers iOS Screen Time shields via the companion BLE protocol;
  local minute countdown; wake shows the last-known locked screen instantly
  ("until 10:30 AM", seeded from NVS) then live-syncs.
* **Priorities** — to-do snapshot from the phone; doubles as the dormant sleep
  screen (fetched pre-sleep even if the app was never opened).
* **Today** — agenda/reminders card from iOS EventKit (`TodayManager`), plus a
  weather band under the header: condition+temp left, high/low right, collapsing
  to nothing when the card carried no weather. The phone produces those two
  strings in `WeatherProvider` (WeatherKit, Open-Meteo as the keyless fallback).
  Needs on-glass validation after Calendar/Reminders permission.
* **Notifications** — iOS notifications via ANCS (incl. iMessage/SMS, which
  arrive as the Social/Other category — no category filtering). Detail view,
  clear-one/clear-all. Newest 10 entries persist across sleep in NVS; on wake
  ANCS re-backfills up to 20 Notification Center items into that seed.
* **Reader** — EPUB library grid + page renderer (section.bin cache, covers,
  12/14/16pt). Procedural book glyph until a bitmap icon is sourced.
* **Workout** — sets-only exercise tracker synced from iPhone; can own the
  sleep face when sleeping from that scene. Procedural dumbbell glyph for now.
* **Settings** opens from the launcher BACK soft-key (not a tile); **About**
  (with wake/heap/battery diagnostics) lives inside Settings. Camera/Inbox/
  Messages were removed to keep the surface focused.

**Clock (v0.4):** `src/Ds3231.{h,cpp}` reads the DS3231 sitting at I2C 0x68 on
the gauge's bus — the chip XteinkDetect already fingerprints. `boot()` seeds
`ClockStore` from it after the X3 guard and before the first paint, so date
attribution, reading streaks and the launcher's `hh:mm` are right with no phone
in range; a stopped oscillator (OSF) falls back to waiting for the phone. The
iPhone's `time.sync` stays the authority (time zone, DST) and rewrites the chip
when they disagree by more than a minute. About reports `clock:`/`rtc:` (state
plus die temperature) — that line is how the backup cell gets tested. No timed
wake: all six wake-capable GPIOs are taken and the battery latch cuts MCU power,
so the sleep frame is stamped `asleep since hh:mm` instead of pretending to tick
(`../docs/lume/12-rtc-e-solo-x3.md` §1.8).

**Power model:** press power = sleep (deep sleep, GPIO3 wake); hold 2.5s =
restart; 10-min idle auto-sleep (2-min during an active block, which does NOT
pin the device awake — the phone enforces the block). Wake restores the
last scene (persisted in NVS — RTC *memory* did NOT survive the X3's power-on-
reset wake). Dormant frame = priorities list + a padlock "Block active until
…" stamp when a block is running.

**Wake is quiet:** resync only the on-glass scene's rail (others sync lazily on
open), retry ≤3, ANCS backfill ≤20; a small sync dot sits left of the battery
while syncing. Per-scene dirty scoping means an unrelated card never repaints
the current scene.

**Measured localized builds:** both use 144,996 B static RAM (44.2% of
327,680 B). Italian uses 2,532,233 B flash and produces a 2,544,768 B image;
English uses 2,531,517 B flash and produces a 2,544,048 B image.

### Dev workflow
* Build both locales: `../.venv/bin/pio run -e lume-x3-it -e lume-x3-en`.
* Host tests (no board, no PlatformIO): `sh test/host/run.sh` — clock/date math
  and DS3231 register handling, both locales, `-Werror`.
* **Flash Italian over USB**: `../.venv/bin/pio run -e lume-x3-it -t upload
  --upload-port /dev/cu.usbmodem*`. The X3 needs a 4-pin data pogo cable.
  Deep sleep drops the USB port; press power to wake before flashing.
* **Serial**: 115200. Capture with a pyserial reset-and-read (DTR/RTS pulse);
  the device is silent when idle (e-ink logs on events only). This is the main
  debugging tool — e.g. the ANCS queue-overflow and encrypted-vs-connected
  resync bugs were both diagnosed live over serial.
* **Flash over SD** (fallback): copy the build to SD root as `update.bin`; the
  boot updater flashes it. SD/reader is flaky — verify (unmount/remount + cmp)
  before trusting. Settings → SD Firmware Update picks any `.bin` (rollback).
* **iOS app**: `xcodebuild -project ios/X4Companion.xcodeproj -scheme
  X4Companion -destination 'id=<UDID>' -allowProvisioningUpdates build` then
  `xcrun devicectl device install app --device <UDID> <app>`.
* **Design first**: `tools/x4-screen-lab` (X3 mode) + `X3-LAYOUT-GUIDE.md`.

### Known TODOs / open notes
* **DS3231 backup cell** — checked on Maurizio's unit (v0.4): OSF read 0 at first
  boot, the chip already held correct local time, and it survived a power-off. On
  any other unit re-run the test: phone connects (writes the clock) → About shows
  `rtc: ok` → power off ten minutes → About again. `rtc: OSF (time lost)` means no
  usable backup cell, and the firmware falls back to waiting for `time.sync`.
* **Today on-glass validation** — iOS EventKit producer shipped (`TodayManager`);
  confirm Calendar/Reminders grant → sync → day buckets/overdue on device, plus
  wake with the NVS-cached Today card, and the weather band (needs a real
  WeatherKit/Open-Meteo fetch on the phone, not just a synced snapshot).
* **Launcher icons** — Block/Today/Notifications sources are still 96px, scaled
  by `tools/xphone-icons/build_launcher_icons.py` to 104px (not 120 — comments
  elsewhere are stale). Re-source those three at ≥208px, keep Priorities weight
  matched, regenerate. Reader/Workout still use procedural glyphs.
* **Notification delta sync** — NVS already seeds the newest 10 across sleep;
  wakes still run a full ≤20 ANCS backfill. Optional: raise persist cap or skip
  UIDs/dates already known.
* Camera could return under Settings if ever wanted.

---

Below is the original M1 scope for reference; see the handoff doc for M2–M3.

The FreeInk SDK is consumed read-only from the sibling `../x4-os` checkout via
symlink lib_deps: `BoardConfig`, `EInkDisplay`, `SDCardManager`,
`InputManager`, `BatteryMonitor`. The two UI fonts are the x4-os builtin
`EpdFontData` headers (Ubuntu 12pt regular/bold), consumed via file symlinks
in `src/fonts/`.

## M1 scope

* **Input** (`src/Input.h`) — logical buttons (Up/Down/Left/Right/Confirm/
  Back) over the SDK `InputManager` ADC ladders. Fixed default mapping (the
  x4-os defaults): front buttons Back/Confirm/Left/Right = hardware 0/1/2/3
  on the GPIO1 ladder, side Up/Down = 4/5 on the GPIO2 ladder. Polled from
  `loop()` every 10ms; 5ms debounce lives in the SDK.
* **Scene manager** (`src/Scene.{h,cpp}`) — one active scene
  (onEnter/onExit/handleInput/render), `switchTo()`, dirty-flag rendering:
  scenes repaint only on state change, never periodically. FULL refresh on
  scene switch, FAST (differential) refresh for selection moves. Single task,
  all scenes static instances, no heap in the loop.
* **Text** (`src/Gfx.{h,cpp}`, `src/Fonts.cpp`) — bespoke ~150-line blitter
  over the raw framebuffer, consuming the SDK's uncompressed EpdFontData
  glyph format directly (packing verified against x4-os
  `GfxRenderer::renderCharImpl`). The full GfxRenderer was rejected: it drags
  x4-os `lib/hal` wholesale (WiFi via HalPowerManager, HalStorage/SdFat via
  Bitmap.h) plus Logging/Utf8/MiniBidi/uzlib. Only the font headers'
  bitmap/glyph/interval arrays are referenced, so their kern matrices
  (~24KB/font) and ligature tables compile out — the whole two-weight font
  stack costs **81,853 bytes** of flash. Kerning/ligatures are skipped (UI
  labels only). The UI renders in **logical portrait** (X3: 528x792, X4:
  480x800, phone-style); `Gfx::drawPixel()` rotates 90 degrees CW into the
  native landscape framebuffer, matching CrossPoint's default
  `GfxRenderer::Portrait` transform.
* **Launcher** (`src/scenes/`) — status bar ("xphone" left, battery percent
  right) over a 3-column app grid: Block, Inbox, Messages, Notifications,
  Camera, Today, Settings, About. Selection = rounded border, moves with
  clamping. Every app opens a shared placeholder scene ("parked for M2")
  except **About**: version, panel, boot-to-first-paint ms, heap
  free/min/largest block.
* **Battery** — included (cheap): SDK `BatteryMonitor`. X3 reads the BQ27220
  I2C fuel gauge (`-DFREEINK_BATTERY_I2C_GAUGE=1`, SDA20/SCL0 per
  BoardConfig); X4 reads the ADC divider on GPIO0. Read failure renders
  `--%`.

## Controls

| Button | Launcher | App / About |
|--------|----------|-------------|
| Up/Down (side) | move selection by row (clamped) | — |
| Left/Right (front) | move selection by column (clamped) | — |
| Confirm (front) | open selected app | — |
| Back (front) | — | return to launcher |

Known M1 limitation: input is not sampled during an e-ink refresh (single
task, no async poll task yet), so presses landing mid-refresh are dropped.

## SD firmware update / recovery

The device has no recovery button flow of its own — **any `update.bin` placed
at the SD-card root is flashed at the next boot**. `src/SdUpdate.cpp` runs
right after display init, before anything else:

1. Mount SD. No card or no `/update.bin` → log and continue normal boot.
2. Validate the image end-to-end (ESP header magic, segment table, XOR
   checksum, SHA256 trailer — the same pass CrossPoint's
   `firmware_flash::validateImageFile` runs) so a truncated/corrupt file is
   never booted.
3. Stream it into the **inactive** OTA app slot (raw `esp_partition` writes,
   4 KiB chunks) while a growing black progress bar is drawn on the e-ink
   (1 full + 4 fast refreshes).
4. Rename `/update.bin` → `/update.bin.flashed` **before** switching the boot
   partition, so the freshly booted firmware never reflashes the same file in
   a loop. (If the rename fails it deletes the file; if neither works the boot
   partition is left untouched.)
5. Write otadata raw to select the new slot (same
   `ota_boot::switchTo` scheme as CrossPoint — the esp_ota_* API is bypassed
   because factory-bootloader-patched images fail `esp_image_verify`), then
   restart.

Any failure logs over serial, draws an X pattern on the panel, leaves the
boot partition untouched, and continues into the normal boot — the device
never bricks.

**Getting back to CrossPoint:** copy a CrossPoint release `firmware.bin`
(the OTA image, offset 0x10000 — the same file CrossPoint's own SD updater
accepts) to the SD root as `update.bin` and reboot. xphone-os flashes it into
the other OTA slot and hands over. To flash the same image again later, rename
`update.bin.flashed` back to `update.bin`.

## Environments

Both environments target the same X3 hardware. Locale is compile-time; each
image contains only its selected language.

| env | locale | device | panel | resolution |
|-----|--------|--------|-------|------------|
| `lume-x3-it` (default) | Italiano | Xteink X3 | UC8253 | 792×528 |
| `lume-x3-en` | English | Xteink X3 | UC8253 | 792×528 |

## Build / flash

```bash
../.venv/bin/pio run -e lume-x3-it -e lume-x3-en
../.venv/bin/pio run -e lume-x3-it -t upload
../.venv/bin/pio device monitor --baud 115200
```

### Releases

Tag `main` with `fw-vX.Y.Z` to publish `update_it.bin`, `update_en.bin`,
locale-specific unversioned/versioned images and zips, checksums, and a
`latest.json` locale manifest.

## Upstream historical milestone record (M1–M5)

| env | RAM | Flash |
|-----|-----|-------|
| `x3` | 22.2% — 72,804 / 327,680 bytes | 7.1% — 462,350 / 6,553,600 bytes |
| `x4` | 20.9% — 68,524 / 327,680 bytes | 7.0% — 460,444 / 6,553,600 bytes |

Delta vs M0.1 (x3): **+404 B RAM, +102,394 B flash** — of which 81,853 B is
the two Ubuntu-12 fonts (bitmaps + glyph tables + interval tables); the rest
is InputManager, BatteryMonitor (+ Wire on x3 for the gauge), and the
scene/launcher code. The ~4.3KB RAM gap between envs is the framebuffer
(X3 792x528/8 = 52,272 B vs X4 800x480/8 = 48,000 B, static in `EInkDisplay`).

## M2 — BLE companion + ANCS rails

**Historical scope.** Upstream originally started the BLE stack after the first
paint and reused the X4 Companion GATT UUIDs, so its existing iOS app connected
unchanged. Current Lume keeps the card/command JSON schema but uses dedicated
service/characteristic UUIDs and a distinct BLE identity; see
`../docs/lume/03-protocollo-ble.md`. Camera image transfer remains stripped. The
firmware also runs an **ANCS client**
(`src/ble/`, ported from x4-os `src/companion/`) that turns the X4 into an
iPhone notification receiver. Completed notifications land in a bounded
`NotificationStore`; the **Notifications** app renders them newest-first
(Up/Down scroll, OK clears, Back home). The launcher status bar shows a
solid dot when the iPhone is connected and a hollow circle while
advertising; About shows BLE/ANCS state, peer address, stored-notification
count and free heap. All BLE payload parsing happens on the main loop —
BLE-host-task callbacks only copy raw bytes into queues (x4-os
`nimble_host` stack-fault learning).

**Pairing flow.** ANCS only works over an encrypted, **bonded** link:

1. Flash and boot; the launcher paints, then the device advertises as
   `X4 Companion`.
2. Connect from the iOS companion app (or iOS Settings > Bluetooth). When
   the X4's ANCS client starts service discovery on the phone, iOS raises a
   **Bluetooth pairing request** popup on the iPhone — accept it. That bonds
   the devices and unlocks the ANCS service; new iPhone notifications then
   stream in (pre-existing/silent ones are deliberately skipped).
3. **Re-pair after trouble** (e.g. `ANCS waiting for encryption` forever, or
   after reflashing wipes the X4's bond table): iPhone Settings > Bluetooth >
   ⓘ next to the X4 > **Forget This Device**, then also clear the X4 side by
   erasing its NVS bond store (`pio run -t erase` then reflash, or wait for a
   Settings app in a later milestone), and connect again.

**Store bounds.** `NotificationStore` is a fixed ring of **32 entries x
232 B** (uid + millis + `appId[32]` + `title[64]` + `message[128]`), ~7.4 KB
static RAM, overwrite-oldest, no heap, no `std::string`. The ported ANCS
client additionally keeps x4-os's 30-slot working set for attribute
assembly; its Data Source reassembly buffer is capped at 512 B and raw
packets are marshalled through a static 6-deep FreeRTOS queue (~3.1 KB).

### Measured size (M2)

| env | RAM | Flash |
|-----|-----|-------|
| `x3` | 29.1% — 95,276 / 327,680 bytes | 13.1% — 859,637 / 6,553,600 bytes |
| `x4` | 27.8% — 91,004 / 327,680 bytes | 13.1% — 857,683 / 6,553,600 bytes |

Delta vs M1 (x3): **+22,472 B RAM, +397,287 B flash** — almost entirely the
NimBLE controller/host + Arduino BLE wrapper; the notification store (7.4 KB)
and ANCS queue (3.1 KB) are the deliberate static costs.

### Soft-key bar (M2.1 chrome)

A persistent bar at the bottom of every scene shows up to 4 small
rounded-top tabs, aligned left-to-right with the 4 physical bottom-front
buttons (SDK ladder order **Back=0, Confirm=1, Left=2, Right=3**, the same
fixed map as `Input.h`). Each scene declares its labels via the
`Scene::softKeys()` virtual (4 static strings; `nullptr` hides a tab);
`SceneManager::renderIfDirty` draws the bar after every scene render so
per-scene chrome cannot drift, and since labels are static per scene it
repaints exactly when the scene does. Scenes reserve the bottom
`Scene::SOFTKEY_BAR_H` (44 px) for it. Current labels:

| scene | Back | Confirm | Left | Right |
|---|---|---|---|---|
| Launcher | — | OPEN | PREV | NEXT |
| Notifications | BACK | CLEAR | UP | DOWN |
| Placeholder / About | BACK | — | — | — |

The launcher grid is now square icon tiles (tile side = column width,
158 px on X3 / 142 px on X4; monogram placeholder icon, label below),
vertically centered between the status bar and the soft-key bar. In
Notifications the front Left/Right buttons scroll (matching their UP/DOWN
tabs) in addition to the top-edge pair.
