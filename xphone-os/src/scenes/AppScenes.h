#pragma once

// xphone-os M1/M2 — scene registry / navigation helpers.
//
// The scene instances live as statics in AppScenes.cpp; these helpers are
// how scenes navigate without including each other's headers.

#include <cstdint>

#include "../games/GameTypes.h"

// Firmware version shown in About (AboutScene.cpp:36), in the Settings header
// and footer (SettingsScene.cpp:287,300) and served by GET /health and
// GET /info (net/FileTransferServer.cpp:70,146,155). A macro, not a constant,
// so the release workflow can inject the released tag with
// -DXPHONE_VERSION='"<tag>"' (.github/workflows/firmware-release.yml) instead
// of shipping an image whose About lies about its own version. Local builds
// pass no flag and keep the default below. The historic symbol name is kept to
// minimise the diff with upstream (docs/lume/CURRENT-STATE.md:49-51).
#ifndef XPHONE_VERSION
#define XPHONE_VERSION "0.1.0-dev"
#endif

// M4.2 last-scene restore: a stable id for each restorable scene. Persisted in
// RTC memory at sleep (Sleep.cpp) and dispatched by boot() on wake so the
// device returns to whatever was on glass. Values are explicit so the
// RTC-stored integer is stable across firmware builds.
enum class SceneId : uint32_t {
  Launcher = 0,
  Notifications = 1,
  Settings = 2,
  Block = 3,
  Reminders = 4,
  Today = 5,
  About = 6,
  Reader = 7,
  Workout = 8,
  FileTransfer = 9,
  // Games: the menu plus one id per pastime. Explicit values like every id
  // above — this integer is what NVS hands back on wake, so renumbering it
  // would restore a device into the wrong app after a firmware update.
  Games = 10,
  Sudoku = 11,
  Nonogram = 12,
  Mines = 13,
  TrailList = 14,
  TrailGame = 15,
};

// Single source of truth for "what scene is on glass" — set by every show*()
// helper below. Read by Sleep::sleepNow() to persist the restore target.
extern SceneId gCurrentSceneId;

// boot() restore dispatch: switch to the scene named by `id` (calls the
// matching show*() so the scene's onEnter re-requests its data). Unknown ids
// fall back to the launcher.
void showSceneById(SceneId id);

// Short human-readable name for a scene id ("Launcher"/"Block"/…), for the
// About wake diagnostic.
const char* sceneName(SceneId id);

void showLauncher();
void showAbout();
void showNotifications();
void showSettings();    // M3: real Settings scene (SD update / restart / about)
void showBlock();       // M3: real Block scene (Screen Time shields via BLE)
void showBlockDeepWork();  // Launcher top-right long-press: open Block + start Deep Work
void showReminders();  // Reminders scene (iOS Reminders lists via BLE)
void showToday();       // M3: real Today scene (agenda/reminders/weather card)
void showReader();      // R1 EPUB reader (resumes the last book; book list on BACK)
void showWorkout();     // Workout: set-by-set exercise tracker synced from iPhone
void showFileTransfer();           // R2: Wi-Fi File Transfer scene (Idle menu)
void showFileTransferAutoStart();  // R2: same, but bring Wi-Fi up immediately (BLE transfer.start)
// R2: BLE "transfer.stop" — ack + restart when the transfer scene is active
// (restart is the clean Wi-Fi teardown); no-op on any other scene.
void stopFileTransferIfActive();

void showGames();       // Games: pick a pastime, then daily or a free-play level
// The three pastimes. `daily` selects today's board (pack index/seed derived
// from the day serial, tier forced to games::kDailyTier and `tier` ignored);
// false is free play at `tier`.
void showSudoku(games::Tier tier, bool daily);
void showNonogram(games::Tier tier, bool daily);
void showMines(games::Tier tier, bool daily);
// Sleep hook: persist the active game's in-progress DAILY board before the loop
// dies. No-op on every other scene.
void gamesPersistDaily();

void showTrailList();
void showTrailStory(const uint8_t* storyData, uint32_t storySize);
void trailPersistSave();
// M2: main.cpp marshals BLE/ANCS events to redraws with these — a scene is
// only marked dirty when it is the one on glass (e-ink discipline: a
// notification burst never repaints the launcher, a connection change never
// repaints Notifications' list rows for nothing).
void markLauncherDirtyIfActive();
void markNotificationsDirtyIfActive();
// M3: companion card revision changed (Block status card updates land here).
void markBlockDirtyIfActive();
// Reminders: revision pump for the reminders snapshot card.
void markRemindersDirtyIfActive();
// M3: same revision pump for the Today snapshot card.
void markTodayDirtyIfActive();
// Workout: revision pump for the workout snapshot card + local +/- bumps.
void markWorkoutDirtyIfActive();

// Total boot-to-first-paint time, set once by main.cpp (shown in About).
extern unsigned long gBootTotalMs;

// M4.2 wake diagnostics — captured ONCE in boot() before anything else can
// change them, rendered on the About scene so we can tell (without a serial
// cable) whether the X3 truly deep-sleeps or cold-boots on power-button wake,
// and whether the last-scene id was captured. gWakeResetReason is a short
// esp_reset_reason() string ("POWERON"/"DEEPSLEEP"/"BROWNOUT"/"SW"/"PANIC"/…);
// gWakeRestoreScene names the restored scene, or "none" on a cold boot.
extern const char* gWakeResetReason;
extern const char* gWakeRestoreScene;
