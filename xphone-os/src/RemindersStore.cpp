#include "RemindersStore.h"

#include <Arduino.h>  // millis() — the pending-tick stamp

#include <cstdio>
#include <cstring>

#include "ble/CompanionProtocol.h"
#include "ble/Utf8Clip.h"

RemindersStore REMINDERS_STORE;

// Keep the fixed fields sized to what the wire protocol can deliver (see the
// header comment) — a protocol bump must grow the store with it.
static_assert(RemindersStore::CAPACITY == CompanionProtocol::MAX_REMINDER_ITEMS, "store capacity != protocol");
static_assert(RemindersStore::LIST_CAPACITY == CompanionProtocol::MAX_REMINDER_LISTS, "list capacity != protocol");
static_assert(sizeof(RemindersStore::Item::title) == CompanionProtocol::MAX_REMINDER_TITLE_CHARS + 1,
              "title field != protocol");
static_assert(sizeof(RemindersStore::Item::due) == CompanionProtocol::MAX_REMINDER_DUE_CHARS + 1,
              "due field != protocol");

// The RAM ceiling this store was designed against: 40 items x 120 B plus the
// list names, the sync line and the assembly cursor. Anything that pushes it
// over must come out of another store, not out of the reader's contiguous
// 32 KB (docs/lume — with BLE connected the largest measured block is ~36 KB).
static_assert(sizeof(RemindersStore) <= 5 * 1024, "reminders store over its 5 KB budget");
static_assert(sizeof(RemindersStore::Item) == 120, "item layout grew; recompute the RAM budget");

void RemindersStore::updateFromCard(const CompanionCardState& card) {
  const char* id = card.id.c_str();
  // A different snapshot id restarts the staging cursor. NOT `part == 0` as
  // PrioritiesStore did: with the parts bitmask below, a re-sent first slice
  // must be ignored, not treated as the start of a new assembly.
  if (std::strcmp(_stageId, id) != 0) {
    snprintf(_stageId, sizeof(_stageId), "%s", id);
    _stageCount = 0;
    _stageListCount = 0;
    _stagePartsSeen = 0;
    _stageGen = 0;
  }

  const int parts = card.parts > 0 ? card.parts : 1;
  int part = card.part;
  if (part < 0 || part >= parts) part = 0;  // malformed index: treat as the only slice
  // 16 bits cover every realistic split (a GATT write caps a card near 512 B,
  // so 40 items land in ~10 parts). Beyond that the bitmask cannot represent
  // the set, so fall back to PrioritiesStore's old "commit on the last part"
  // rule rather than never committing.
  const bool tracked = parts <= 16;
  if (tracked) {
    const uint16_t bit = static_cast<uint16_t>(1u << part);
    if (_stagePartsSeen & bit) return;  // duplicate slice, or an id already committed
    _stagePartsSeen |= bit;
  }

  // Lists and body ride in EVERY part (identical each time), so a part that
  // omits them keeps what was staged.
  for (std::size_t i = 0; i < card.reminderListCount && i < LIST_CAPACITY; i++) {
    if (card.reminderListNames[i].empty()) continue;
    // clipUtf8 clips on a codepoint boundary and always NUL-terminates: the
    // card cap (24 B) equals this slot, but a name that arrives at the cap
    // must not lose half of an accented sequence.
    clipUtf8(_listNames[i], sizeof(_listNames[i]), card.reminderListNames[i].c_str());
    if (i + 1 > _stageListCount) _stageListCount = static_cast<uint8_t>(i + 1);
  }
  _stageGen = card.reminderGeneration;

  for (std::size_t i = 0; i < card.reminderItemCount && _stageCount < CAPACITY; i++) {
    const CompanionReminderItem& src = card.reminderItems[i];
    if (src.handle == 0) continue;  // 0 is not a valid handle: the row could never be toggled
    Item& dst = _items[_stageCount++];
    dst.handle = src.handle;
    dst.list = src.list < LIST_CAPACITY ? src.list : 0;
    clipUtf8(dst.title, sizeof(dst.title), src.title.c_str());
    clipUtf8(dst.due, sizeof(dst.due), src.due.c_str());
    // The phone sends open reminders only, so a fresh slice always lands open;
    // this also clears whatever pending tick the slot held for the previous
    // snapshot's reminder.
    dst.pendingDone = false;
    dst.pendingSinceMs = 0;
  }

  // Commit only when the whole set is in: count/list count/generation/sync
  // line/revision move together, so a render between slices sees the OLD
  // list's length, never a half-assembled one.
  const uint16_t full = tracked ? static_cast<uint16_t>(parts >= 16 ? 0xffffu : ((1u << parts) - 1u)) : 0;
  const bool complete = tracked ? (_stagePartsSeen == full) : (part + 1 >= parts);
  if (!complete) return;

  _count = _stageCount;
  _listCount = _stageListCount;
  _gen = _stageGen;
  // The card body carries up to MAX_BODY_CHARS (512 B) but the sync line slot
  // is 97 B, so this copy is a real cut: clip on a codepoint boundary
  // (ble/Utf8Clip.h) so an accented "3 da fare" line never renders half a
  // sequence. Always NUL-terminates.
  clipUtf8(_syncLine, sizeof(_syncLine), card.body.c_str());
  _revision++;
  _stagePartsSeen = 0xffff;  // finished: later copies of this id are ignored
}

bool RemindersStore::get(std::size_t index, Item& out) const {
  if (index >= _count) return false;
  out = _items[index];
  return true;
}

const char* RemindersStore::listName(std::size_t listIndex) const {
  if (listIndex >= _listCount) return "";
  return _listNames[listIndex];
}

std::size_t RemindersStore::countForList(std::size_t listIndex) const {
  if (listIndex >= LIST_CAPACITY) return 0;
  std::size_t n = 0;
  for (std::size_t i = 0; i < _count; i++) {
    if (_items[i].list == listIndex) n++;
  }
  return n;
}

bool RemindersStore::getInList(std::size_t listIndex, std::size_t indexInList, Item& out) const {
  if (listIndex >= LIST_CAPACITY) return false;
  std::size_t seen = 0;
  for (std::size_t i = 0; i < _count; i++) {
    if (_items[i].list != listIndex) continue;
    if (seen++ != indexInList) continue;
    out = _items[i];
    return true;
  }
  return false;
}

bool RemindersStore::markPendingDone(uint16_t handle) {
  if (handle == 0) return false;
  for (std::size_t i = 0; i < _count; i++) {
    if (_items[i].handle != handle || _items[i].pendingDone) continue;
    _items[i].pendingDone = true;
    _items[i].pendingSinceMs = static_cast<uint16_t>(millis());
    return true;
  }
  return false;
}

std::size_t RemindersStore::expirePending(uint32_t nowMs) {
  const uint16_t now = static_cast<uint16_t>(nowMs);
  std::size_t restored = 0;
  for (std::size_t i = 0; i < _count; i++) {
    if (!_items[i].pendingDone) continue;
    // Modular 16-bit difference (see Item::pendingSinceMs).
    if (static_cast<uint16_t>(now - _items[i].pendingSinceMs) < PENDING_TIMEOUT_MS) continue;
    _items[i].pendingDone = false;
    _items[i].pendingSinceMs = 0;
    restored++;
  }
  return restored;
}
