#include "AppScenes.h"
#include "../LumeLocale.h"

#include "AboutScene.h"
#include "BlockScene.h"
#include "FileTransferScene.h"
#include "GamesScene.h"
#include "LauncherScene.h"
#include "MinesScene.h"
#include "NonogramScene.h"
#include "NotificationsScene.h"
#include "RemindersScene.h"
#include "ReaderScene.h"
#include "SettingsScene.h"
#include "SudokuScene.h"
#include "TodayScene.h"
#include "TrailListScene.h"
#include "TrailScene.h"
#include "WorkoutScene.h"
#include "WifiScene.h"
unsigned long gBootTotalMs = 0;

// M4.2 last-scene restore: updated by every show*() helper below.
SceneId gCurrentSceneId = SceneId::Launcher;

// M4.2 wake diagnostics — set once by main.cpp boot(); defaults hold until then.
const char* gWakeResetReason = "?";
const char* gWakeRestoreScene = L10N("none", "nessuna");

namespace {
// All scenes are static instances — fixed allocation, zero heap churn.
LauncherScene gLauncher;
AboutScene gAbout;
NotificationsScene gNotifications;
SettingsScene gSettings;
BlockScene gBlock;
RemindersScene gReminders;
TodayScene gToday;
WorkoutScene gWorkout;
ReaderScene gReader;
FileTransferScene gFileTransfer;
GamesScene gGames;
SudokuScene gSudoku;
NonogramScene gNonogram;
MinesScene gMines;
  TrailListScene gTrailList;
  TrailScene gTrailScene;
  WifiScene gWifi;
}  // namespace

void showLauncher() {
  gCurrentSceneId = SceneId::Launcher;
  SCENES.switchTo(gLauncher);
}

void showAbout() {
  gCurrentSceneId = SceneId::About;
  SCENES.switchTo(gAbout);
}

void showNotifications() {
  gCurrentSceneId = SceneId::Notifications;
  SCENES.switchTo(gNotifications);
}

void showSettings() {
  gCurrentSceneId = SceneId::Settings;
  SCENES.switchTo(gSettings);
}

void showBlock() {
  gCurrentSceneId = SceneId::Block;
  SCENES.switchTo(gBlock);
}

void showBlockDeepWork() {
  gCurrentSceneId = SceneId::Block;
  SCENES.switchTo(gBlock);  // runs onEnter (fresh status request)
  gBlock.startDeepWork();   // then fire block.start(deep_work) immediately
}

void showReminders() {
  gCurrentSceneId = SceneId::Reminders;
  SCENES.switchTo(gReminders);
}

void showToday() {
  gCurrentSceneId = SceneId::Today;
  SCENES.switchTo(gToday);
}

void showReader() {
  gCurrentSceneId = SceneId::Reader;
  SCENES.switchTo(gReader);
}

void showWorkout() {
  gCurrentSceneId = SceneId::Workout;
  SCENES.switchTo(gWorkout);
}

void showFileTransfer() {
  gCurrentSceneId = SceneId::FileTransfer;
  SCENES.switchTo(gFileTransfer);
}

void showFileTransferAutoStart() {
  SCENES.switchTo(gFileTransfer);  // onEnter resets to Idle
  gFileTransfer.autoStart();       // then bring the radio up (STA or hotspot)
}

void showFileTransferAutoStartDirect() {
  gCurrentSceneId = SceneId::FileTransfer;
  SCENES.switchTo(gFileTransfer);
  gFileTransfer.autoStartDirect();
}

void stopFileTransferIfActive() {
  if (SCENES.active() == &gFileTransfer) gFileTransfer.stopSession();
}

void showWifi() {
  gCurrentSceneId = SceneId::Wifi;
  SCENES.switchTo(gWifi);
}

void showSceneByIdQuiet(const SceneId id) {
  showSceneById(id);
}
void showGames() {
  gCurrentSceneId = SceneId::Games;
  SCENES.switchTo(gGames);
}

// The three pastimes. switchTo() runs onEnter FIRST, so open() has to come
// after it or the fresh board it builds would be wiped by the scene's own
// reset — the same ordering showBlockDeepWork() depends on above.
void showSudoku(const games::Tier tier, const bool daily) {
  gCurrentSceneId = SceneId::Sudoku;
  SCENES.switchTo(gSudoku);
  gSudoku.open(tier, daily);
}

void showNonogram(const games::Tier tier, const bool daily) {
  gCurrentSceneId = SceneId::Nonogram;
  SCENES.switchTo(gNonogram);
  gNonogram.open(tier, daily);
}

void showMines(const games::Tier tier, const bool daily) {
  gCurrentSceneId = SceneId::Mines;
  SCENES.switchTo(gMines);
  gMines.open(tier, daily);
}

void showTrailList() {
  gCurrentSceneId = SceneId::TrailList;
  SCENES.switchTo(gTrailList);
}

void showTrailStory(const uint8_t* storyData, uint32_t storySize) {
  gCurrentSceneId = SceneId::TrailGame;
  SCENES.switchTo(gTrailScene);
  gTrailScene.open(storyData, storySize);
}

void trailPersistSave() {
  if (SCENES.active() == &gTrailScene) {
    gTrailScene.persistSave();
  }
}

// Sleep hook (Sleep::sleepNow): the in-progress DAILY board is the one piece of
// game state that cannot be regenerated, so it goes to NVS before the loop dies.
void gamesPersistDaily() {
  Scene* const active = SCENES.active();
  if (active == &gSudoku) {
    gSudoku.persistDaily();
  } else if (active == &gNonogram) {
    gNonogram.persistDaily();
  } else if (active == &gMines) {
    gMines.persistDaily();
  } else if (active == &gTrailScene) {
    gTrailScene.persistSave();
  }
}
// boot() restore dispatch. Each show*() re-runs the scene's onEnter, which
// re-requests its companion data (Block/Priorities/Today/Notifications all do),
// so a restored scene refreshes itself. Sub-view state (Settings picker,
// Notifications detail, Block modes/break) resets to the scene default — fine.
void showSceneById(SceneId id) {
  switch (id) {
    case SceneId::Notifications: showNotifications(); break;
    case SceneId::Settings:      showSettings();      break;
    case SceneId::Block:         showBlock();         break;
    case SceneId::Reminders:     showReminders();     break;
    case SceneId::Today:         showToday();         break;
    case SceneId::About:         showAbout();         break;
    case SceneId::Reader:        showReader();        break;
    case SceneId::Workout:       showWorkout();       break;
    case SceneId::Wifi:          showWifi();          break;
    case SceneId::Games:         showGames();         break;
    // only board that was persisted (free play is disposable by design), and
    // waking straight back into it is the whole point of having saved it.
    case SceneId::Sudoku:        showSudoku(games::kDailyTier, true);   break;
    case SceneId::Nonogram:      showNonogram(games::kDailyTier, true); break;
    case SceneId::Mines:         showMines(games::kDailyTier, true);    break;
    case SceneId::TrailList:
    case SceneId::TrailGame:     showTrailList();                       break;
    // FileTransfer deliberately NOT restored: waking straight into a scene
    // that would show a stale Idle menu (the radio never survives sleep)
    // helps nobody — fall through to the launcher.
    case SceneId::FileTransfer:
    case SceneId::Launcher:
    default:                     showLauncher();      break;
  }
}

const char* sceneName(SceneId id) {
  switch (id) {
    case SceneId::Notifications: return L10N("Notifications", "Notifiche");
    case SceneId::Settings:      return L10N("Settings", "Impostazioni");
    case SceneId::Block:         return L10N("Block", "Focus");
    case SceneId::Reminders:     return L10N("Reminders", "Promemoria");
    case SceneId::Today:         return L10N("Today", "Oggi");
    case SceneId::About:         return L10N("About", "Informazioni");
    case SceneId::Reader:        return L10N("Reader", "Lettura");
    case SceneId::Workout:       return L10N("Workout", "Allenamento");
    case SceneId::FileTransfer:  return L10N("Transfer", "Trasferimento");
    case SceneId::Games:         return L10N("Games", "Giochi");
    case SceneId::Sudoku:        return L10N("Sudoku", "Sudoku");
    case SceneId::Nonogram:      return L10N("Nonogram", "Nonogram");
    case SceneId::Mines:         return L10N("Mines", "Campo minato");
    case SceneId::TrailList:     return L10N("Adventures", "Avventure");
    case SceneId::TrailGame:     return L10N("Adventure", "Avventura");
    case SceneId::Launcher:      return L10N("Launcher", "Home");
    default:                     return L10N("Launcher", "Home");
  }
}

void markLauncherDirtyIfActive() {
  if (SCENES.active() == &gLauncher) gLauncher.markDirty();
}

void markNotificationsDirtyIfActive() {
  if (SCENES.active() == &gNotifications) gNotifications.markDirty();
}

void markBlockDirtyIfActive() {
  if (SCENES.active() == &gBlock) gBlock.markDirty();
}

void markRemindersDirtyIfActive() {
  if (SCENES.active() == &gReminders) gReminders.markDirty();
}

void markTodayDirtyIfActive() {
  if (SCENES.active() == &gToday) gToday.markDirty();
}

void markWorkoutDirtyIfActive() {
  if (SCENES.active() == &gWorkout) gWorkout.markDirty();
}
