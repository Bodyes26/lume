# Lume

Lume is a personal, Xteink X3-only fork of
[Flowe OS](https://github.com/andrewjiang/flowe-os). It keeps the upstream
ESP32-C3 firmware architecture and companion protocol while moving toward an
Italian, configurable desk dashboard and a new iOS companion app.

**Current state:** firmware `0.1.0-dev` is accepted on an X3; the native SwiftUI
Lume companion now implements the first v0.2 vertical slice: CoreBluetooth
discovery/restoration, time sync, and persistent Priorities snapshot/toggle.
Today, Workout, Block, reader transfer and Screen Time control remain on the
roadmap. The EPUB reader works independently from the phone.

Development decisions, verified architecture notes and the cross-session
handoff start at [`docs/lume/START-HERE.md`](docs/lume/START-HERE.md).

<p align="center">
  <img src="docs/screens/launcher.png" width="240" alt="Launcher" />
  <img src="docs/screens/priorities.png" width="240" alt="Priorities" />
  <img src="docs/screens/block.png" width="240" alt="Block" />
</p>

The screenshots above document the inherited upstream UI; visible branding is
being replaced incrementally.

## The apps

| | |
|---|---|
| **Priorities** | Speak your morning priorities into the iPhone app; they arrive on ink. Check them off with one key. |
| **Today** | The next 24 hours of your calendar, at a glance. No feed underneath. |
| **Notifications** | Mirrored from your iPhone over ANCS — the same channel a smartwatch uses. Real app names, and missed notifications backfill on reconnect. |
| **Block** | Press the device's side button (or start from the app) and iOS Screen Time shields the apps you chose to block until the countdown ends. The countdown lives on ink; daily count and streak too. |
| **Read** | A full EPUB reader — typeset pages, covers, remembers your place. Books load from the SD card. Works with no phone at all. |
| **Workout** | Your plan for the session; one key press logs a set. The phone stays wherever you left it. |

<p align="center">
  <img src="docs/screens/today.png" width="180" alt="Today" />
  <img src="docs/screens/notifications.png" width="180" alt="Notifications" />
  <img src="docs/screens/reader.png" width="180" alt="Reader" />
  <img src="docs/screens/workout.png" width="180" alt="Workout" />
</p>

## Functional sleep

The device spends most of its life asleep, so the sleep screen earns its
keep: today's priorities as a poster, your next calendar event, and the
state of any running block. Before the panel powers down, the firmware
snapshots the current scene, block state, synced cards, and recent
notifications to on-chip storage (reading position lives on the SD card) —
so wake picks up where you left off.

<p align="center">
  <img src="docs/screens/poster.png" width="240" alt="Sleep screen" />
</p>

## Companion protocol

Lume remains wire-compatible with the upstream app during the fork transition.
It advertises as **`Lume X3`**, but iOS discovers it through the unchanged GATT
service UUID rather than the display name. One bonded BLE link carries:

- **ANCS**, Apple's public notification service;
- **JSON cards and actions** for priorities, agenda, workouts and block state;
- **ephemeral Wi-Fi credentials and transfer commands** for EPUB sync.

The complete implementer-facing specification for the new iOS app, including
UUIDs, payload schemas, limits, ordering and reconnect behavior, is
[`docs/lume/03-protocollo-ble.md`](docs/lume/03-protocollo-ble.md).
The reader itself needs no companion app.

## iOS companion

The native app lives in `ios/`. It targets iOS 17+, stores up to ten priorities
locally, reconnects to the last X3 through CoreBluetooth state restoration, and
serializes every GATT write with a response and an 8-second recovery timeout.
The firmware service UUID remains upstream-compatible; the user-visible device
name is `Lume X3`.

Generate, build and test it with:

```sh
cd ios
xcodegen generate
xcodebuild build -project Lume.xcodeproj -target Lume \
  -configuration Debug -sdk iphoneos CODE_SIGNING_ALLOWED=NO
swift test
```

Installing on an iPhone requires an Apple account selected for team
`PRF667R7JB` in Xcode and an automatically generated development provisioning
profile for `com.maurizio.lume`. The Simulator validates the visual shell but
cannot exercise CoreBluetooth.

## Performance

The whole OS — BLE, ANCS, EPUB reader, framebuffer — fits in the ESP32-C3's
320 KB of RAM with no PSRAM: the current X3 build uses about 146 KB of RAM
and 2.5 MB of its 6.5 MB app partition.

- **Input stays responsive.** A dedicated 5 ms sampling task latches key
  presses even while the panel is mid-refresh, and display flushes run on a
  worker task so the main loop keeps composing and pumping BLE.
- **Fast, calm refreshes.** Scene changes use the panel's fast waveform
  (~450 ms on the X3, versus ~3 s for a full flash); small updates use
  driver-native partial windows, with periodic full-panel scrubs to keep
  the glass clean.
- **Quiet wake.** Press power to sleep; wake resyncs only what the current
  screen needs and backfills missed notifications, newest first.

## Install

No public Lume release has been published yet. For development, build
`xphone-os/.pio/build/lume-x3/firmware.bin`, or rename that image to
`update.bin` and copy it to the root of the microSD card. Insert the card and
hold **Left + Power**. The updater validates the image before switching the
inactive OTA slot.

The X3 fingerprint guard runs before display initialization. If the image is
accidentally installed on non-X3 hardware, Lume leaves the panel untouched and
halts for USB recovery.

## Build from source

The repository-local Python environment keeps the developer toolchain separate
from the rest of the workstation:

```sh
python3.11 -m venv .venv
.venv/bin/python -m pip install platformio==6.1.19
cd xphone-os
../.venv/bin/pio run -e lume-x3
../.venv/bin/pio run -e lume-x3 -t upload
../.venv/bin/pio device monitor --baud 115200
```

USB flashing needs the 4-pin data pogo cable — the 2-pin cable bundled with
many X3s is charge-only, so use the SD-card path if the device never
enumerates.

`freeink-sdk/` is the vendored [FreeInk SDK](https://opencollective.com/freeink)
(MIT) — board, display, SD, input, and battery drivers. The EPUB engine is
adapted from [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader)
(MIT).

## License

Lume and the inherited Flowe OS firmware are MIT-licensed — see [LICENSE](LICENSE). Vendored libraries under
`xphone-os/lib/` and `freeink-sdk/` retain their original licenses
(MIT, Apache-2.0, and Zlib).
