#include "AppScenes.h"
#include "../LumeLocale.h"

#include "AboutScene.h"
#include "BlockScene.h"
#include "FileTransferScene.h"
#include "LauncherScene.h"
#include "NotificationsScene.h"
#include "RemindersScene.h"
#include "ReaderScene.h"
#include "SettingsScene.h"
#include "TodayScene.h"
#include "WorkoutScene.h"

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
  gCurrentSceneId = SceneId::FileTransfer;
  SCENES.switchTo(gFileTransfer);  // onEnter resets to Idle
  gFileTransfer.autoStart();       // then bring the radio up (STA or hotspot)
}

void stopFileTransferIfActive() {
  if (SCENES.active() == &gFileTransfer) gFileTransfer.stopAndRestart();
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
