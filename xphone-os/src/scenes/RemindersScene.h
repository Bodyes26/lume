#pragma once

// Lume — Reminders: the iPhone's Reminders lists on glass, and the one place
// the day's to-dos live. Successor of the Priorities scene (deleted): the
// local priorities database is gone, iOS Reminders are the single source of
// truth, and ticking a row here writes to EventKit through the phone — so the
// same tick shows up in Reminders on iPhone/Mac/Watch.
//
// Wire contract (protocol constants in ble/CompanionProtocol.h, producer in
// ios/Lume/RemindersStore.swift): the phone pushes a "reminders.snapshot"
// card carrying `gen` (uint16 handle-map generation), `reminderLists`
// ([listIndex, name], part 0 only) and `reminderItems`
// ([handle, listIndex, title, dueLabel]). ONLY OPEN reminders travel — there
// is no `done` field, so a ticked row simply stops arriving in the next
// snapshot. The device answers with "reminder.toggle" (gen + handle + done)
// and "reminders.sync.request"; the phone ignores a toggle whose `gen` no
// longer matches (the handle map moved under us) and replies with a fresh
// snapshot instead.
//
// CONTROLS. The selected lists are TABS here, which is the one deliberate
// break with the Priorities muscle memory: Priorities mapped front
// Left/Right to UP/DOWN as a second way to move the selection
// (PrioritiesScene.cpp handleInput), but with up to 4 lists something has to
// switch tab, and the front pair sits directly under the two right-hand
// soft-key tabs — the same place the launcher puts PREV/NEXT
// (LauncherScene::softKeys). So:
//   * top-edge Up/Down  -> move the selection, paging automatically when it
//                          leaves the visible window (NotificationsScene's
//                          moveSelection discipline)
//   * front slot 1      -> CONFIRM: DONE (tick the selected reminder), or
//                          SYNC while the active list is empty — exactly what
//                          Priorities did with an empty list
//   * front slot 2 / 3  -> PREV / NEXT list (hidden when only one list is
//                          selected on the phone: no indicator, no dead tabs)
//   * slot 0            -> BACK to the launcher
//
// A tick is OPTIMISTIC and never blocks input: markPendingDone() flags the row
// (drawn ticked + a "sending" marker), and the row stays pending until the
// next snapshot drops it. RemindersStore::expirePending() reverts it after
// 8 s — the same deadline as the app's GATT queue — and the scene says so on
// its status line. Priorities had no deadline at all: a lost toggle left the
// row lying about itself until the user re-synced.
//
// All list state lives in REMINDERS_STORE (fixed char buffers, filled by the
// companion service for every snapshot, even ones landing while another scene
// is on glass), so handleInput()/softKeys() read it directly on the 10 ms
// input tick with zero heap copies.

#include "../Scene.h"

class RemindersScene : public Scene {
 public:
  void onEnter() override;
  void handleInput(Input& in) override;
  void render(Gfx& gfx) override;
  const char* const* softKeys() const override;

  // Dormant sleep frame — the poster the glass holds all night. Composes
  // (into the already cleared framebuffer): today's date, the next timed
  // calendar event, up to 3 DATED to-dos, the block line when a block is
  // active, and the wake hint. Returns false when there is neither an event
  // nor a dated to-do — the caller then keeps the plain wordmark screen.
  // Never flushes; Sleep.cpp owns the FULL refresh.
  static bool renderDormant(Gfx& gfx);

  // Sleep-frame chrome shared with Sleep.cpp's wordmark/workout faces so all
  // three read as one design. Centered one-liners drawn at `y`, each false
  // when it had nothing to draw:
  //  - Block line: lock + "Until 17:30 | Today: 2" while a block is active,
  //    or lock + "Today: 2" after completions.
  //  - Calendar line: calendar glyph + the next TIMED event from the Today
  //    store ("20:00: Cena"). All-day entries (birthdays etc.) are skipped —
  //    the line answers "what's next on the clock".
  static bool renderDormantBlockLine(Gfx& gfx, int y);
  static bool renderDormantFooter(Gfx& gfx, int y);

  // Sleep-frame vertical grid, as offsets from the panel bottom, so every
  // sleep face stacks identically and nothing lands below the poster ceiling
  // (panelH - Scene::SOFTKEY_BAR_H = 748 on the X3's 528x792 logical panel).
  // Height budget in RemindersScene.cpp's renderDormant() comment.
  static constexpr int dormantEventY(int panelH) { return panelH - 172; }  // upper slot
  static constexpr int dormantBlockY(int panelH) { return panelH - 128; }  // lower slot
  static constexpr int dormantHintY(int panelH) { return panelH - 72; }    // 720 + 24 = 744

 private:
  void toggleSelected();
  void moveSelection(int delta);
  void switchList(int delta);
  // Active tab, clamped to what the store currently holds (a snapshot with
  // fewer lists can land while this scene is on glass).
  int activeList() const;
  XpRect contentRect() const;  // sub-header + status line + rows
  XpRect listRect() const;     // rows only
  XpRect rowRect(int visibleIndex) const;
  XpRect headerRect() const;

  int _sel = 0;     // selection WITHIN the active list
  int _scroll = 0;  // first visible row of the active list
  int _list = 0;    // active tab; kept across visits (the phone's list order is stable)
  const char* _localMsg = "";       // static literals only
  uint32_t _seenStoreRevision = 0;  // REMINDERS_STORE.revision() last consumed by render()
  int _rowsPerPageCache = 1;
  int16_t _wCache = 0, _hCache = 0;  // panel dims cached by render()
};
