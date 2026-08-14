#include "RemindersScene.h"

#include <cstdio>
#include <cstring>

#include "../BlockStatusStore.h"
#include "../ClockStore.h"
#include "../Fonts.h"
#include "../RemindersStore.h"
#include "../SyncIndicator.h"
#include "../TodayStore.h"
#include "../ble/CompanionBleService.h"
#include "AppScenes.h"

namespace {

constexpr int kMarginX = 20;
constexpr int kHeaderH = 46;  // same chrome as NotificationsScene/BlockScene
constexpr int kCheckboxSize = 32;

// The three UI fonts have FIXED advances (Fonts.cpp: ubuntu_12 bold/regular
// advanceY 29, ubuntu_10 small 24), so the whole row grid is a compile-time
// constant — rowRect() and moveSelection() can address rows on the 10 ms input
// tick without a Gfx in hand (NotificationsScene::rowRect does the same).
constexpr int kSubY = kHeaderH + 10;             // list name / tab indicator line: 56
constexpr int kStatusY = kSubY + 29 + 2;         // transient + range line: 87
constexpr int kListTop = kStatusY + 24 + 12;     // first row: 123
constexpr int kRowH = 29 + 24 + 22;              // title + due + padding: 75

// Sleep-poster grid (see the height budget in renderDormant()).
constexpr int kDormDateY = 88;
constexpr int kDormRuleGap = 10;
constexpr int kDormRuleW = 56;
constexpr int kDormBandTop = 152;
constexpr int kDormEventSlotH = 40;  // kFontRegular 29 + breathing room
constexpr int kDormEventGap = 24;
constexpr int kDormRowH = 48;        // checkbox 32 + breathing room
constexpr int kDormRows = 3;         // the user's call: 3 dated to-dos, no more

// Width-clipping copy (same helper as NotificationsScene) — titles come from
// the phone and can exceed a row.
void truncateToWidth(Gfx& gfx, const XpFont& font, const char* src, int maxWidth, char* dst, size_t dstSize) {
  snprintf(dst, dstSize, "%s", src ? src : "");
  if (gfx.textWidth(font, dst) <= maxWidth) return;
  size_t len = strlen(dst);
  while (len > 0) {
    do {
      len--;
    } while (len > 0 && (static_cast<uint8_t>(dst[len]) & 0xC0) == 0x80);
    dst[len] = '\0';
    char probe[160];
    snprintf(probe, sizeof(probe), "%s...", dst);
    if (gfx.textWidth(font, probe) <= maxWidth) {
      snprintf(dst, dstSize, "%s", probe);
      return;
    }
  }
}

// CrossPoint's 32x32 rounded checkbox (x4-os PrioritiesActivity.cpp
// drawCheckbox: 28-33, border 3 when selected else 2). The tick is a real
// two-stroke diagonal check via Gfx::drawLine — short down-right stroke into
// the notch, long up-right stroke out of it, 3px stamp, centered inside the
// border. Here `checked` means "optimistically ticked, waiting for the phone"
// (RemindersStore::Item::pendingDone): the snapshot only ever carries OPEN
// reminders, so a confirmed tick simply stops arriving.
void drawCheckbox(Gfx& gfx, const int x, const int y, const bool checked, const int thickness) {
  gfx.drawRoundedRect(x, y, kCheckboxSize, kCheckboxSize, 8, thickness, true);
  if (!checked) return;
  gfx.drawLine(x + 8, y + 16, x + 13, y + 22, 3, true);   // down-right into the notch
  gfx.drawLine(x + 13, y + 22, x + 24, y + 10, 3, true);  // long up-right stroke
}

// Small padlock glyph for the dormant BLOCK stamp — the same primitives as
// BlockScene::drawLock (rounded shackle + filled body + carved keyhole) but
// sized to stand next to stamp text (~d px tall). Always locked (a block is
// active whenever this is drawn). `d` is the glyph height budget.
void drawLockGlyph(Gfx& gfx, const int x, const int y, const int d) {
  // Taller-than-wide proportions (body w = 3/4 d) so the padlock reads as a
  // padlock next to a full text line instead of a squashed square.
  const int bodyW = (d * 3) / 4;
  const int bodyH = (d * 9) / 16;            // body ~56% of the height
  const int bodyX = x + (d - bodyW) / 2;
  const int bodyY = y + d - bodyH;           // body sits at the bottom of the budget
  const int shackleW = (bodyW * 3) / 5;      // shackle narrower than the body
  const int shackleX = x + (d - shackleW) / 2;
  const int shackleH = d - bodyH + (bodyH / 3);  // dips into the body so the arc reads closed
  gfx.drawRoundedRect(shackleX, y, shackleW, shackleH, shackleW / 2, 2, true);
  gfx.fillRoundedRect(bodyX, bodyY, bodyW, bodyH, 3, true);
  const int khW = d / 5;                     // keyhole carved white
  gfx.fillRoundedRect(x + (d - khW) / 2, bodyY + bodyH / 4, khW, khW, khW / 2, false);
  gfx.fillRect(x + d / 2 - 1, bodyY + bodyH / 4 + khW / 2, 2, bodyH / 3, false);
}

// Calendar glyph for the dormant next-event line: page slightly taller than
// wide (w = 7/8 d), filled header band, two binding tabs, four date dots.
// `d` is the HEIGHT budget; occupies w x d at (x, y).
void drawCalendarGlyph(Gfx& gfx, const int x, const int y, const int d) {
  const int w = (d * 7) / 8;
  const int bodyY = y + 4;
  const int bodyH = d - 4;                              // page fills the height
  gfx.drawRoundedRect(x, bodyY, w, bodyH, 4, 2, true);
  gfx.fillRect(x + 2, bodyY + 2, w - 4, (d * 5) / 16, true);  // header band
  gfx.fillRect(x + w / 4 - 1, y, 3, 8, true);           // left binding tab
  gfx.fillRect(x + (3 * w) / 4 - 2, y, 3, 8, true);     // right binding tab
  const int dotTop = bodyY + (d * 5) / 16 + 5;          // date dots, 2x2 grid
  const int dotGapX = w / 3;
  const int dotGapY = (bodyY + bodyH - 5) - dotTop;
  gfx.fillRect(x + w / 4, dotTop, 3, 3, true);
  gfx.fillRect(x + w / 4 + dotGapX, dotTop, 3, 3, true);
  if (dotGapY >= 6) {
    gfx.fillRect(x + w / 4, dotTop + dotGapY / 2, 3, 3, true);
    gfx.fillRect(x + w / 4 + dotGapX, dotTop + dotGapY / 2, 3, 3, true);
  }
}

// The next TIMED calendar event. The phone sends the rolling now->+24h agenda
// sorted by start, so the first timed item IS the next event; all-day entries
// (birthdays etc. arrive with time "All day") have no clock and are skipped.
// No reminder filter any more: since the Reminders cutover the today snapshot
// carries agenda events ONLY (TodayScene.cpp, ios RemindersStore.swift owns
// reminders), so the old kind == "reminder" skip was dead weight.
bool nextTimedEvent(TodayStore::Item& out) {
  for (std::size_t i = 0; i < TODAY_STORE.count(); i++) {
    if (!TODAY_STORE.get(i, out)) continue;
    if (!out.time[0] || strcmp(out.time, "All day") == 0) continue;
    return out.title[0] != '\0';
  }
  return false;
}

}  // namespace

void RemindersScene::onEnter() {
  // Selection restarts at the top of the active list; the TAB is deliberately
  // kept across visits (the phone sends the lists in the order the user picked
  // in the app, so index 1 is still "Casa" next time) and clamped by
  // activeList() when a shorter snapshot lands.
  _sel = 0;
  _scroll = 0;
  // Ask for a fresh snapshot so the list is not stale from a previous session
  // (the same entry behavior Priorities had — main.cpp owns BLE bring-up).
  if (COMPANION_BLE.isConnected() && COMPANION_BLE.sendRemindersSyncRequest()) {
    _localMsg = L10N("Requesting reminders...", "Richiedo i promemoria...");
  } else {
    // No transient in flight — leave the line empty so render() derives it live
    // from the connection ("Syncing..." vs "Connect Companion") and it can't go
    // stale after the link comes up post-wake.
    _localMsg = "";
  }
}

const char* const* RemindersScene::softKeys() const {
  // Slot 1 is DONE while the active list has something to tick, SYNC when it
  // is empty (Priorities' behavior with an empty list). Slots 2/3 are the tab
  // pair, hidden entirely with a single list so there is no dead chrome.
  static constexpr const char* kDoneTabs[4] = {L10N("BACK", "INDIETRO"), L10N("DONE", "FATTO"),
                                               L10N("PREV", "PREC"), L10N("NEXT", "SUCC")};
  static constexpr const char* kDoneOnly[4] = {L10N("BACK", "INDIETRO"), L10N("DONE", "FATTO"), nullptr, nullptr};
  static constexpr const char* kSyncTabs[4] = {L10N("BACK", "INDIETRO"), L10N("SYNC", "SINC"),
                                               L10N("PREV", "PREC"), L10N("NEXT", "SUCC")};
  static constexpr const char* kSyncOnly[4] = {L10N("BACK", "INDIETRO"), L10N("SYNC", "SINC"), nullptr, nullptr};
  const bool tabs = REMINDERS_STORE.listCount() > 1;
  const bool canTick = REMINDERS_STORE.countForList(static_cast<std::size_t>(activeList())) > 0;
  if (canTick) return tabs ? kDoneTabs : kDoneOnly;
  return tabs ? kSyncTabs : kSyncOnly;
}

int RemindersScene::activeList() const {
  const int lists = static_cast<int>(REMINDERS_STORE.listCount());
  if (lists <= 0) return 0;
  return _list < lists ? _list : 0;
}

XpRect RemindersScene::headerRect() const {
  if (_hCache <= 0) return XpRect{};  // no layout yet -> full-panel fallback
  return XpRect{0, 0, _wCache, kHeaderH};
}

XpRect RemindersScene::contentRect() const {
  if (_hCache <= 0) return XpRect{};
  return XpRect{0, kHeaderH, _wCache, static_cast<int16_t>(_hCache - kHeaderH - Scene::SOFTKEY_BAR_H)};
}

XpRect RemindersScene::listRect() const {
  if (_hCache <= 0) return XpRect{};
  const int top = kListTop - 8;  // the selected row's border draws above kListTop
  return XpRect{0, static_cast<int16_t>(top), _wCache,
                static_cast<int16_t>(_hCache - Scene::SOFTKEY_BAR_H - top)};
}

XpRect RemindersScene::rowRect(const int visibleIndex) const {
  if (_hCache <= 0 || visibleIndex < 0 || visibleIndex >= _rowsPerPageCache) return XpRect{};
  const int y = kListTop + visibleIndex * kRowH;
  // 8px slop above/below: the selection border is 3px and draws outside the
  // text block, and the row separator sits 3px under the card.
  return XpRect{0, static_cast<int16_t>(y - 8), _wCache, static_cast<int16_t>(kRowH + 8)};
}

void RemindersScene::moveSelection(const int delta) {
  const int inList = static_cast<int>(REMINDERS_STORE.countForList(static_cast<std::size_t>(activeList())));
  const int next = _sel + delta;
  if (next < 0 || next >= inList) return;  // clamp, no wrap (Priorities clamped too)
  const int prev = _sel;
  _sel = next;
  const int oldScroll = _scroll;
  // Automatic paging when the selection leaves the window (NotificationsScene
  // moveSelection discipline) — the top-edge pair is the only way to scroll now
  // that the front pair switches tabs.
  if (_sel < _scroll) _scroll = _sel;
  if (_sel >= _scroll + _rowsPerPageCache) _scroll = _sel - _rowsPerPageCache + 1;
  if (_scroll != oldScroll) {
    markDirty(contentRect());  // rows shifted AND the "1-8 of 12" range moved
    return;
  }
  // Same page: repaint only the two affected rows.
  XpRect dirty = rowRect(prev - _scroll);
  dirty.unionWith(rowRect(_sel - _scroll));
  markDirty(dirty);
}

void RemindersScene::switchList(const int delta) {
  const int lists = static_cast<int>(REMINDERS_STORE.listCount());
  if (lists <= 1) return;  // single list: the tab keys are not even drawn
  int next = activeList() + delta;
  if (next < 0) next = lists - 1;  // wrap, like the launcher's PREV/NEXT pair
  if (next >= lists) next = 0;
  _list = next;
  _sel = 0;
  _scroll = 0;
  markDirty(contentRect());  // list name, counts and every row change
}

void RemindersScene::toggleSelected() {
  RemindersStore::Item item;
  if (_sel < 0 ||
      !REMINDERS_STORE.getInList(static_cast<std::size_t>(activeList()), static_cast<std::size_t>(_sel), item)) {
    return;
  }
  if (item.handle == 0) return;      // 0 is not a valid handle — nothing to address
  if (item.pendingDone) return;      // already waiting for this one; a second toggle would double-write
  // `gen` pins the toggle to the handle map the phone built for THIS snapshot:
  // if the list moved under us the phone drops the toggle and answers with a
  // fresh snapshot instead of ticking the wrong reminder.
  if (COMPANION_BLE.sendReminderToggle(item.handle, REMINDERS_STORE.generation(), true)) {
    // Optimistic: the row shows ticked + "sending" immediately and stays that
    // way until the next snapshot drops it, or expirePending() reverts it 8 s
    // from now (deadline owned by the store).
    REMINDERS_STORE.markPendingDone(item.handle);
    _localMsg = L10N("Ticking on iPhone...", "Spunto sull'iPhone...");
  } else {
    _localMsg = L10N("Connect Companion to update.", "Collega Lume per aggiornare.");
  }
  markDirty(contentRect());
}

void RemindersScene::handleInput(Input& in) {
  // Header transfer-arrow flash: bold on a new sent/received transient, back
  // to the thin idle pair ~1s later. Header-window repaint only.
  if (SyncIndicator::tick(COMPANION_BLE.getRevision(), millis(),
                          [] { return COMPANION_BLE.getStatusMessage(); })) {
    markDirty(headerRect());
  }

  // 8 s optimistic-tick deadline: polled on the input tick, so a toggle the
  // phone never answered self-heals (row back to open) with the status line
  // saying why — and nothing was ever blocked while we waited.
  if (REMINDERS_STORE.expirePending(millis()) > 0) {
    _localMsg = L10N("iPhone did not answer: reminder restored.", "L'iPhone non ha risposto: voce ripristinata.");
    markDirty(contentRect());
  }

  if (in.wasPressed(Btn::Back)) {
    showLauncher();
    return;
  }

  // The store is main-loop-only fixed state — these are plain member reads on
  // the 10 ms tick, no heap copies.
  const int inList = static_cast<int>(REMINDERS_STORE.countForList(static_cast<std::size_t>(activeList())));
  if (_sel > inList - 1) _sel = inList > 0 ? inList - 1 : 0;

  if (in.wasPressed(Btn::Confirm)) {
    if (inList > 0) {
      toggleSelected();  // DONE: write the tick through to EventKit via the phone
    } else {
      // SYNC: re-request the snapshot. Empty line when offline so render()
      // derives the live Syncing/Connect message.
      _localMsg = COMPANION_BLE.sendRemindersSyncRequest() ? L10N("Requesting reminders...", "Richiedo i promemoria...")
                                                           : "";
      markDirty();
    }
    return;
  }

  // Front Left/Right = PREV/NEXT list. This is the ONE break with the
  // Priorities muscle memory (there they were a second UP/DOWN pair): the tabs
  // need a switch, and these two buttons sit directly under the soft-key tabs
  // that now read PREV/NEXT.
  if (in.wasPressed(Btn::Left)) {
    switchList(-1);
    return;
  }
  if (in.wasPressed(Btn::Right)) {
    switchList(+1);
    return;
  }

  // Top-edge pair = selection, with automatic paging.
  if (in.wasPressed(Btn::Up)) moveSelection(-1);
  if (in.wasPressed(Btn::Down)) moveSelection(+1);
}

void RemindersScene::render(Gfx& gfx) {
  const int w = gfx.width();
  const int h = gfx.height();
  _wCache = static_cast<int16_t>(w);
  _hCache = static_cast<int16_t>(h);

  // A FRESH snapshot (the store's revision moves only when one lands) is the
  // phone's answer to any in-flight sync/toggle — clear the transient message.
  const uint32_t storeRevision = REMINDERS_STORE.revision();
  if (storeRevision != _seenStoreRevision) {
    _seenStoreRevision = storeRevision;
    _localMsg = "";
  }

  // --- Header: title + companion transfer glyph (same chrome as BlockScene) --
  gfx.drawText(kFontBold, kMarginX, 8, L10N("Reminders", "Promemoria"));
  // Transfer transients render as the paired up/down arrows; routine BLE status
  // ("Paired & encrypted", advertising lines) stays OFF the header — it read as
  // noise. Connect problems surface in the empty state and the About scene.
  const std::string msg = COMPANION_BLE.getStatusMessage();
  SyncIndicator::draw(gfx, w - kMarginX, 8, gfx.lineHeight(kFontRegular), msg.c_str());
  gfx.fillRect(0, kHeaderH - 2, w, 2, true);

  // --- Never synced: revision 0 means no snapshot has EVER landed -----------
  if (storeRevision == 0) {
    const int cy = h / 2;
    gfx.drawTextCentered(kFontBold, w / 2, cy - 2 * gfx.lineHeight(kFontBold),
                         L10N("No reminders yet", "Ancora nessun promemoria"));
    // An in-flight transient ("Requesting reminders...") wins; otherwise
    // "Syncing..." while the link is up (main.cpp's auto-resync has re-requested
    // and the phone is about to answer), else the connect hint.
    const char* line = _localMsg[0]                  ? _localMsg
                       : COMPANION_BLE.isConnected() ? L10N("Syncing...", "Sincronizzo...")
                                                     : L10N("Connect Companion to sync.", "Collega Lume per sincronizzare.");
    gfx.drawTextCentered(kFontRegular, w / 2, cy - gfx.lineHeight(kFontRegular) / 2, line);
    // The lists themselves are chosen in the app — say so, or the empty screen
    // looks like a broken sync.
    gfx.drawTextCentered(kFontSmall, w / 2, cy + gfx.lineHeight(kFontRegular) + 6,
                         L10N("Pick your lists in the Lume app.", "Scegli le liste nell'app Lume."));
    return;
  }

  const int lists = static_cast<int>(REMINDERS_STORE.listCount());
  const int tab = activeList();
  const int inList = static_cast<int>(REMINDERS_STORE.countForList(static_cast<std::size_t>(tab)));
  if (_sel > inList - 1) _sel = inList > 0 ? inList - 1 : 0;

  // --- Sub-header: active list name + tab indicator + count ------------------
  // With a single list there is no "1/1" — an indicator that can never change
  // is noise (same reason the tab soft-keys disappear).
  char meta[40];
  if (lists > 1) {
    snprintf(meta, sizeof(meta), L10N("%d/%d · %d to do", "%d/%d · %d da fare"), tab + 1, lists, inList);
  } else {
    snprintf(meta, sizeof(meta), L10N("%d to do", "%d da fare"), inList);
  }
  const int metaW = gfx.textWidth(kFontRegular, meta);
  char name[64];
  truncateToWidth(gfx, kFontBold, REMINDERS_STORE.listName(static_cast<std::size_t>(tab)),
                  w - 2 * kMarginX - metaW - 16, name, sizeof(name));
  gfx.drawText(kFontBold, kMarginX, kSubY, name[0] ? name : L10N("Reminders", "Promemoria"));
  gfx.drawText(kFontRegular, w - kMarginX - metaW, kSubY, meta);

  // --- Status line: transients only ("Ticking on iPhone...") ----------------
  if (_localMsg[0]) {
    char status[96];
    truncateToWidth(gfx, kFontSmall, _localMsg, w - 2 * kMarginX - 96, status, sizeof(status));
    gfx.drawText(kFontSmall, kMarginX, kStatusY, status);
  }

  int perPage = (h - Scene::SOFTKEY_BAR_H - 8 - kListTop) / kRowH;
  if (perPage < 1) perPage = 1;
  _rowsPerPageCache = perPage;

  // --- Empty list: say it, never hand the user a blank screen ---------------
  if (inList == 0) {
    _scroll = 0;
    const int cx = w / 2;
    const int ty = kListTop + 40;
    gfx.drawTextCentered(kFontRegular, cx, ty, L10N("This list is clear.", "Questa lista è a posto."));
    gfx.drawTextCentered(kFontSmall, cx, ty + gfx.lineHeight(kFontRegular) + 6,
                         lists > 1 ? L10N("PREV / NEXT for another list.", "PREC / SUCC per un'altra lista.")
                                   : L10N("SYNC to ask the iPhone again.", "SINC per richiedere all'iPhone."));
    return;
  }

  // --- Rows ------------------------------------------------------------------
  // Scroll window follows the selection (moveSelection keeps them in step; this
  // re-clamps after a snapshot shortened the list under us).
  if (_sel < _scroll) _scroll = _sel;
  if (_sel >= _scroll + perPage) _scroll = _sel - perPage + 1;
  const int maxScroll = inList > perPage ? inList - perPage : 0;
  if (_scroll > maxScroll) _scroll = maxScroll;
  if (_scroll < 0) _scroll = 0;

  if (inList > perPage) {  // range indicator, right-aligned on the status line
    char range[24];
    const int last = _scroll + perPage < inList ? _scroll + perPage : inList;
    snprintf(range, sizeof(range), L10N("%d-%d of %d", "%d-%d di %d"), _scroll + 1, last, inList);
    gfx.drawText(kFontSmall, w - kMarginX - gfx.textWidth(kFontSmall, range), kStatusY, range);
  }

  const int rowW = w - 2 * kMarginX;
  int y = kListTop;
  for (int i = _scroll; i < inList && i - _scroll < perPage; i++) {
    RemindersStore::Item item;
    if (!REMINDERS_STORE.getInList(static_cast<std::size_t>(tab), static_cast<std::size_t>(i), item)) break;
    const bool selected = i == _sel;
    const int cardH = kRowH - 8;

    if (selected) {
      gfx.drawRoundedRect(kMarginX, y, rowW, cardH, 14, 3, true);
    } else {
      gfx.fillRect(kMarginX + 12, y + cardH + 3, rowW - 24, 1, true);  // separator
    }

    drawCheckbox(gfx, kMarginX + 14, y + (cardH - kCheckboxSize) / 2, item.pendingDone, selected ? 3 : 2);

    const int textX = kMarginX + 14 + kCheckboxSize + 16;
    const int textW = w - kMarginX - 14 - textX;
    const XpFont& titleFont = selected ? kFontBold : kFontRegular;
    // A reminder with no due date is a single-line row: the title alone,
    // centered in the card level with the checkbox.
    const int textBlockH = gfx.lineHeight(titleFont) + (item.due[0] ? gfx.lineHeight(kFontSmall) : 0);
    const int textTop = y + (cardH - textBlockH) / 2;

    int titleMax = textW;
    if (item.pendingDone) {
      // Waiting marker: the tick is already drawn, this says the phone has not
      // confirmed it yet (and expirePending may still take it back).
      const char* mark = L10N("sending...", "invio...");
      const int markW = gfx.textWidth(kFontSmall, mark);
      gfx.drawText(kFontSmall, textX + textW - markW,
                   textTop + (gfx.lineHeight(titleFont) - gfx.lineHeight(kFontSmall)) / 2, mark);
      titleMax -= markW + 12;
    }

    char title[112];
    truncateToWidth(gfx, titleFont, item.title, titleMax, title, sizeof(title));
    gfx.drawText(titleFont, textX, textTop, title);

    if (item.due[0]) {
      char due[32];
      truncateToWidth(gfx, kFontSmall, item.due, textW, due, sizeof(due));
      gfx.drawText(kFontSmall, textX, textTop + gfx.lineHeight(titleFont), due);
    }
    y += kRowH;
  }
}

// Dormant sleep frame — the poster the glass holds all night, redesigned for
// the Reminders cutover: the date, what is next on the clock, and what is left
// to do with a time on it. No "Today's priorities" title any more (the frame IS
// the day) and no moon/wordmark stamp (Sleep.cpp's wordmark face covers the
// nothing-to-say case). The frame is frozen until the next wake, so the drawing
// time is stamped by Sleep::drawSleepScreen rather than a clock that would go
// stale here.
//
// HEIGHT BUDGET — X3 logical panel 528 x 792, poster ceiling
// 792 - Scene::SOFTKEY_BAR_H = 748. The "asleep since hh:mm" stamp at y 12..36
// is Sleep.cpp's; we never draw in that band.
//   date        88..117   kFontBold, advanceY 29
//   rule       127..129   2px
//   band       152..696   (152..644 when the block line is drawn), content
//                         vertically centered inside it; max content
//                         40 + 24 + 3*48 = 208
//   block      664..693   dormantBlockY(792) = 664, kFontRegular 29
//   wake hint  720..744   dormantHintY(792) = 720, kFontSmall 24  ->  < 748 OK
bool RemindersScene::renderDormant(Gfx& gfx) {
  const int w = gfx.width();
  const int h = gfx.height();
  const int cx = w / 2;

  // Only DATED to-dos make the poster: the frame is frozen for hours, and a
  // reminder with no time says nothing about tonight or tomorrow morning.
  int picks[kDormRows];
  int picked = 0;
  RemindersStore::Item item;
  const int count = static_cast<int>(REMINDERS_STORE.count());
  for (int i = 0; i < count && picked < kDormRows; i++) {
    if (!REMINDERS_STORE.get(static_cast<std::size_t>(i), item)) break;
    if (!item.due[0]) continue;
    picks[picked++] = i;
  }

  TodayStore::Item event;
  const bool haveEvent = nextTimedEvent(event);
  // Neither an appointment nor a dated to-do: nothing worth a poster — the
  // caller keeps the plain wordmark screen (which draws the same block line).
  if (picked == 0 && !haveEvent) return false;

  char date[24];
  const bool haveDate = clockFormatShortDate(date, sizeof(date));
  if (haveDate) {
    gfx.drawTextCentered(kFontBold, cx, kDormDateY, date);
    gfx.fillRect(cx - kDormRuleW / 2, kDormDateY + gfx.lineHeight(kFontBold) + kDormRuleGap, kDormRuleW, 2, true);
  }

  const bool blockDrawn = renderDormantBlockLine(gfx, dormantBlockY(h));

  const int bandTop = haveDate ? kDormBandTop : kDormDateY;
  const int bandBottom = blockDrawn ? dormantBlockY(h) - 20 : dormantHintY(h) - 24;
  const int groupH = (haveEvent ? kDormEventSlotH : 0) + (haveEvent && picked > 0 ? kDormEventGap : 0) +
                     picked * kDormRowH;
  int y = bandTop + (bandBottom - bandTop - groupH) / 2;
  if (y < bandTop) y = bandTop;

  if (haveEvent) {
    renderDormantFooter(gfx, y);  // calendar glyph + "20:00: Cena"
    y += kDormEventSlotH + (picked > 0 ? kDormEventGap : 0);
  }

  const int boxX = kMarginX + 28;
  const int textX = boxX + kCheckboxSize + 16;
  for (int i = 0; i < picked; i++) {
    if (!REMINDERS_STORE.get(static_cast<std::size_t>(picks[i]), item)) break;
    drawCheckbox(gfx, boxX, y + (kDormRowH - kCheckboxSize) / 2, item.pendingDone, 2);
    // Due label right-aligned so the times line up in a column; the title takes
    // whatever is left.
    const int dueW = gfx.textWidth(kFontSmall, item.due);
    char title[112];
    truncateToWidth(gfx, kFontBold, item.title, w - kMarginX - 28 - dueW - 16 - textX, title, sizeof(title));
    gfx.drawText(kFontBold, textX, y + (kDormRowH - gfx.lineHeight(kFontBold)) / 2, title);
    gfx.drawText(kFontSmall, w - kMarginX - 28 - dueW, y + (kDormRowH - gfx.lineHeight(kFontSmall)) / 2, item.due);
    y += kDormRowH;
  }

  gfx.drawTextCentered(kFontSmall, cx, dormantHintY(h), L10N("press power to wake", "premi accensione"));
  return true;
}

// Centered block line: lock + "Until 17:30 | Today: 2" while a block is active
// (falling back to a minutes-left line when the phone omits the end-time
// label), or lock + "Today: 2" after completions. False when there is nothing
// to say.
bool RemindersScene::renderDormantBlockLine(Gfx& gfx, const int y) {
  const BlockStatusStore::Status block = BLOCK_STATUS.get();
  char label[48];
  if (block.active) {
    char when[24];
    if (block.endsAtLabel[0]) {
      snprintf(when, sizeof(when), L10N("Until %s", "Fino alle %s"), block.endsAtLabel);
    } else {
      snprintf(when, sizeof(when), L10N("%d min left", "%d min rimasti"), block.remainingMinutes);
    }
    snprintf(label, sizeof(label), L10N("%s | Today: %d", "%s | Oggi: %d"), when, block.blocksToday);
  } else if (block.blocksToday > 0) {
    snprintf(label, sizeof(label), L10N("Today: %d", "Oggi: %d"), block.blocksToday);
  } else {
    return false;
  }
  constexpr int kLockD = 24;
  constexpr int kGap = 10;
  const int groupW = kLockD + kGap + gfx.textWidth(kFontRegular, label);
  const int gx = gfx.width() / 2 - groupW / 2;
  drawLockGlyph(gfx, gx, y + (gfx.lineHeight(kFontRegular) - kLockD) / 2, kLockD);
  gfx.drawText(kFontRegular, gx + kLockD + kGap, y, label);
  return true;
}

// Centered calendar line: glyph + the next timed event ("20:00: Cena"). No day
// prefix — the Today producer only sends the rolling now->+24h window, so the
// time is unambiguous and the saved width goes to the event title. False when
// no timed event is known.
bool RemindersScene::renderDormantFooter(Gfx& gfx, const int y) {
  TodayStore::Item item;
  if (!nextTimedEvent(item)) return false;

  char label[128];
  snprintf(label, sizeof(label), "%s: %s", item.time, item.title);

  const int cx = gfx.width() / 2;
  constexpr int kCalD = 28;
  constexpr int kGap = 12;
  char clipped[112];
  truncateToWidth(gfx, kFontRegular, label, gfx.width() - 2 * kMarginX - kCalD - kGap, clipped, sizeof(clipped));
  const int groupW = kCalD + kGap + gfx.textWidth(kFontRegular, clipped);
  const int gx = cx - groupW / 2;
  drawCalendarGlyph(gfx, gx, y + (gfx.lineHeight(kFontRegular) - kCalD) / 2, kCalD);
  gfx.drawText(kFontRegular, gx + kCalD + kGap, y, clipped);
  return true;
}
