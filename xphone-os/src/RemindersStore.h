#pragma once

// Lume — dedicated iOS Reminders store (replaces the deleted PrioritiesStore).
//
// The app-local priorities database is gone: iOS Reminders is now the single
// source of truth, the phone pushes ONLY OPEN reminders, and ticking a row on
// glass writes back through EventKit. So there is no `done` field on the wire
// and no local list to reconcile — a checked reminder simply is not in the
// next snapshot.
//
// NotificationStore's mold, as PrioritiesStore had it: fixed char fields, no
// std::string, no heap growth. Sized against the RAM ceiling in
// docs/lume (144,996 B static of 327,680, and the reader needs a CONTIGUOUS
// 32 KB heap block while BLE is up): the static_assert at the bottom of the
// .cpp pins the whole store under 5 KB, and the ~2.9 KB PrioritiesStore it
// replaces is gone.
//
// The companion service copies every reminders snapshot into this store at the
// parse point (CompanionBleService.cpp applyCardPayload), so a snapshot that
// lands while another scene is on glass is still captured — the service keeps
// exactly ONE card slot and the next Block/Today push clobbers it. Consumers
// are the Reminders scene and the dormant sleep frame.
//
// applyCardPayload runs ONLY on the Arduino main loop (handleCardWrite on the
// NimBLE host task just stashes raw bytes; processPending() parses them from
// loop()/Sleep.cpp), and scenes + Sleep render on the main loop too, so every
// accessor here — including the optimistic-toggle mutators — is
// main-loop-only by construction: no mutex needed.

#include <cstddef>
#include <cstdint>

struct CompanionCardState;

class RemindersStore {
 public:
  // Capacity/field sizes mirror CompanionProtocol (MAX_REMINDER_ITEMS,
  // MAX_REMINDER_LISTS, MAX_REMINDER_TITLE_CHARS + NUL,
  // MAX_REMINDER_DUE_CHARS + NUL, MAX_REMINDER_LIST_NAME_CHARS + NUL);
  // static_asserts in the .cpp keep them in lockstep.
  static constexpr std::size_t CAPACITY = 40;
  static constexpr std::size_t LIST_CAPACITY = 4;

  // Optimistic-toggle deadline. Matches the 8 s the iOS app's GATT queue gives
  // a write: past it the phone either never got the toggle or never answered,
  // so the row must go back to open rather than lie about a reminder that is
  // still sitting in Reminders.app.
  static constexpr uint16_t PENDING_TIMEOUT_MS = 8000;

  struct Item {
    uint16_t handle = 0;  // phone-side handle (1..65535); what reminder.toggle sends back
    uint8_t list = 0;     // 0..LIST_CAPACITY-1, index into listName()
    char title[97] = {0};
    char due[17] = {0};   // already formatted + localized by the phone; "" when undated
    bool pendingDone = false;
    // Low 16 bits of millis() when the row was ticked. 16 bits, not 32, so
    // Item stays at the 120 B the RAM budget was computed against; the
    // modular difference is exact for any elapsed time under 65.5 s and
    // expirePending() runs on every main-loop tick (main.cpp
    // pumpCompanionEvents), so the wrap is unreachable in practice.
    uint16_t pendingSinceMs = 0;
  };  // 120 bytes

  // Copy the card's reminder lists + items + generation and its body line into
  // the fixed buffers, and bump the revision. Call ONLY when the card actually
  // is a reminders snapshot — the service's predicate decides.
  //
  // Multi-part snapshots (card.parts > 1, slices sharing one card id) stage
  // into the item slots per part and commit count/list count/generation/sync
  // line/revision together on the slice that COMPLETES the set, tracked as a
  // bitmask of parts seen. So parts arriving out of order still commit exactly
  // once, a duplicated slice is ignored instead of appended twice, and a LOST
  // slice never commits at all: the previous snapshot stays on glass until the
  // next sync repairs it, rather than a list truncated to the parts that made
  // it through.
  void updateFromCard(const CompanionCardState& card);

  std::size_t count() const { return _count; }
  // Bounds-checked copy-out; returns false when index >= count().
  bool get(std::size_t index, Item& out) const;

  // Lists are the device's tabs. listCount() is 0 until the first snapshot.
  std::size_t listCount() const { return _listCount; }
  // "" when listIndex is past the lists this snapshot carried.
  const char* listName(std::size_t listIndex) const;
  std::size_t countForList(std::size_t listIndex) const;
  // n-th item (0-based) of listIndex, in the order the phone sent it.
  bool getInList(std::size_t listIndex, std::size_t indexInList, Item& out) const;

  // Generation of the phone's handle->reminder map. Echoed in every
  // reminder.toggle so the phone can reject a toggle whose handles came from a
  // list it has since rebuilt (CompanionBleService::sendReminderToggle).
  uint16_t generation() const { return _gen; }

  // The snapshot card's body line ("" until the first snapshot lands).
  const char* syncLine() const { return _syncLine; }

  // Bumped ONLY by a committed snapshot — never by the optimistic-toggle
  // mutators below, so the scene can use `revision() == 0` as "never synced"
  // and main.cpp/Sleep.cpp can poll it to detect the phone's reply.
  uint32_t revision() const { return _revision; }

  // --- Optimistic toggle ----------------------------------------------------
  // Tick the row NOW so e-ink feedback is immediate, and remember when: the
  // phone's EventKit write plus the snapshot round trip takes seconds on the
  // low-duty link. Returns false when no OPEN item carries that handle
  // (already pending, or the list moved under the caller).
  bool markPendingDone(uint16_t handle);
  // Revert every pending tick older than PENDING_TIMEOUT_MS and return how
  // many rows went back to open — nonzero means the scene must repaint and say
  // the update did not land. Called from the main loop (main.cpp), which is
  // why the caller passes millis() instead of the store reading the clock.
  std::size_t expirePending(uint32_t nowMs);

 private:
  Item _items[CAPACITY];
  char _listNames[LIST_CAPACITY][25] = {};
  char _syncLine[97] = {0};
  // Multi-part assembly cursor: the card id the staged slices belong to, how
  // many item slots they filled, and which parts have arrived.
  char _stageId[65] = {0};
  std::size_t _count = 0;
  std::size_t _stageCount = 0;
  uint32_t _revision = 0;
  uint16_t _gen = 0;
  uint16_t _stageGen = 0;
  // Bit per part index; 0xffff also serves as the "this id already committed,
  // ignore further slices" sentinel (see updateFromCard).
  uint16_t _stagePartsSeen = 0;
  uint8_t _listCount = 0;
  uint8_t _stageListCount = 0;
};

extern RemindersStore REMINDERS_STORE;
