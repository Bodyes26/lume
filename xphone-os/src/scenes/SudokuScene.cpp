#include "SudokuScene.h"

#include <Arduino.h>
#include <esp_random.h>

#include <cstdio>

#include "../ClockStore.h"
#include "../Fonts.h"
#include "../games/GameStats.h"
#include "AppScenes.h"

namespace {

constexpr int kMarginX = 20;
constexpr int kHeaderH = 46;  // separator rule at 44, content from 46 — same chrome as every app card

// Grid: 9 x 52 = 468 px wide, so 30 px of side margin on the X3's 528 (the
// widest cell that still leaves the digit strip a full row below it).
constexpr int kCell = 52;
constexpr int kGridX = 30;
constexpr int kGridY = 62;
constexpr int kGridSide = games::Sudoku::kSide * kCell;  // bottom edge at 530
constexpr int kBoxLine = 3;                              // 3x3 separators + outer border
constexpr int kRingPad = 3;                              // cursor ring sits 3 px outside the cell
constexpr int kMarkInset = 10;                           // conflict underline: 32 px centred in the cell
constexpr int kMarkW = 32;
constexpr int kMarkY = 44;

// Digit strip: 10 x 42 + 9 x 4 = 456, centred (36 px margins).
constexpr int kSlotCount = 10;
constexpr uint8_t kClearSlot = kSlotCount - 1;
constexpr int kSlotW = 42;
constexpr int kSlotH = 48;
constexpr int kSlotGap = 4;
constexpr int kSlotRadius = 8;
constexpr int kStripX = 36;
constexpr int kStripY = 556;
constexpr int kStripW = kSlotCount * kSlotW + (kSlotCount - 1) * kSlotGap;

// Status lines. The lower one ends at 654 + 24 = 678, comfortably above the
// soft-key bar's 748 (792 - Scene::SOFTKEY_BAR_H).
constexpr int kStatusY = 620;
constexpr int kSubY = 654;
constexpr int kStatusTop = kStatusY - 4;
constexpr int kStatusBottom = kSubY + 24 + 4;

// Direction buttons, in _rpt[] order. Left/Right are front keys under the
// PREV/NEXT tabs, Up/Down the two side buttons.
enum Dir : uint8_t { kDirLeft = 0, kDirRight, kDirUp, kDirDown };
constexpr Btn kDirBtn[4] = {Btn::Left, Btn::Right, Btn::Up, Btn::Down};

int cellLeft(const int col) { return kGridX + col * kCell; }
int cellTop(const int row) { return kGridY + row * kCell; }
int slotLeft(const int i) { return kStripX + i * (kSlotW + kSlotGap); }

// Cursor-ring bounds, i.e. the smallest window a cursor step or a digit write
// can change.
XpRect cellRect(const int cell) {
  const int x = cellLeft(cell % games::Sudoku::kSide) - kRingPad;
  const int y = cellTop(cell / games::Sudoku::kSide) - kRingPad;
  return XpRect{static_cast<int16_t>(x), static_cast<int16_t>(y), kCell + 2 * kRingPad, kCell + 2 * kRingPad};
}

XpRect stripRect() { return XpRect{kStripX, kStripY, kStripW, kSlotH}; }

XpRect statusRect(const int16_t w) {
  return XpRect{0, kStatusTop, w, static_cast<int16_t>(kStatusBottom - kStatusTop)};
}

const char* tierLabel(const games::Tier tier) {
  switch (tier) {
    case games::Tier::Easy:
      return L10N("Easy", "Facile");
    case games::Tier::Hard:
      return L10N("Hard", "Difficile");
    case games::Tier::Medium:
    default:
      return L10N("Medium", "Medio");
  }
}

}  // namespace

void SudokuScene::open(const games::Tier tier, const bool daily) {
  // Idempotent after the first call, so the scene never depends on who else
  // touched NVS first.
  GAME_STATS.load();

  uint32_t ymd = 0;
  uint16_t mins = 0;
  _day = clockNow(ymd, mins) ? clockSerialFromYmd(ymd) : 0;
  // No clock (no DS3231 seed, no phone yet) means no day to belong to: the
  // board is still playable, it just isn't anybody's daily.
  _daily = daily && _day != 0;
  _tier = _daily ? games::kDailyTier : tier;

  if (_daily) {
    _model.load(_tier, games::dailyIndex(_day, games::Sudoku::packCount(_tier), games::kSaltSudoku));
    uint8_t blob[games::Sudoku::kBlobBytes];
    if (GAME_STATS.loadDaily(games::Game::Sudoku, _day, blob, sizeof blob) == sizeof blob) {
      _model.restore(blob);  // a refused blob (stale day, regenerated pack) leaves the fresh grid
    }
  } else {
    _model.load(_tier, esp_random());
  }

  _mode = Mode::Board;
  _cursor = 0;
  _digitSel = 0;
  markDirty();
}

void SudokuScene::persistDaily() {
  if (!_daily) return;
  // A solved daily has already been cleared by markDailyDone; re-saving it
  // would resurrect a finished grid on the next open.
  if (_model.solved()) return;
  uint8_t blob[games::Sudoku::kBlobBytes];
  _model.snapshot(blob);
  GAME_STATS.saveDaily(games::Game::Sudoku, _day, blob, sizeof blob);
}

void SudokuScene::onEnter() { markDirty(); }

void SudokuScene::onExit() { persistDaily(); }

const char* const* SudokuScene::softKeys() const {
  static constexpr const char* kBoard[4] = {L10N("BACK", "INDIETRO"), L10N("DIGIT", "CIFRA"), L10N("PREV", "PREC"),
                                            L10N("NEXT", "SUCC")};
  static constexpr const char* kDigit[4] = {L10N("CANCEL", "ANNULLA"), L10N("OK", "OK"), L10N("PREV", "PREC"),
                                            L10N("NEXT", "SUCC")};
  static constexpr const char* kWonFree[4] = {L10N("BACK", "INDIETRO"), L10N("NEW", "NUOVO"), nullptr, nullptr};
  static constexpr const char* kWonDaily[4] = {L10N("BACK", "INDIETRO"), nullptr, nullptr, nullptr};

  if (_model.solved()) return _daily ? kWonDaily : kWonFree;
  return _mode == Mode::Digit ? kDigit : kBoard;
}

// Slot 0 is the OS-wide long-press BACK dot; slot 1 clears the cursor cell.
uint8_t SudokuScene::longPressSlots() const { return 0x01 | 0x02; }

void SudokuScene::moveCursor(const int delta) {
  const uint8_t from = _cursor;
  _cursor = static_cast<uint8_t>((_cursor + games::Sudoku::kCells + delta) % games::Sudoku::kCells);
  if (_cursor == from) return;
  markDirty(cellRect(from));  // two cell windows, not the grid: the ring moved, the digits did not
  markDirty(cellRect(_cursor));
}

void SudokuScene::moveSlot(const int delta) {
  _digitSel = static_cast<uint8_t>((_digitSel + kSlotCount + delta) % kSlotCount);
  markDirty(stripRect());
}

void SudokuScene::enterDigitMode() {
  const uint8_t value = _model.at(_cursor);
  _digitSel = value ? static_cast<uint8_t>(value - 1) : 0;  // the cell's own digit, else 1
  _mode = Mode::Digit;
  markDirty(stripRect());
  markDirty(cellRect(_cursor));  // the ring thins: focus left the board
}

void SudokuScene::leaveDigitMode() {
  _mode = Mode::Board;
  markDirty(stripRect());
  markDirty(cellRect(_cursor));
}

void SudokuScene::commitSlot() {
  const uint8_t value = (_digitSel == kClearSlot) ? 0u : static_cast<uint8_t>(_digitSel + 1);
  // Givens make set() return false; writing the digit that is already there
  // changes nothing either. Either way the CELL must stay still.
  const bool wrote = _model.at(_cursor) != value && _model.set(_cursor, value);

  // The mode change is real even when the write was refused — leaving the
  // strip's filled slot on glass would be a lie about where focus is.
  leaveDigitMode();
  if (!wrote) return;

  markDirty(statusRect(_wCache));  // "cells left" moved
  if (_model.solved()) recordWin();
}

void SudokuScene::clearCursorCell() {
  if (_model.at(_cursor) == 0) return;   // already empty, or a given the model will refuse
  if (!_model.set(_cursor, 0)) return;   // given: keep the panel still
  markDirty(cellRect(_cursor));
  markDirty(statusRect(_wCache));  // no strip in this path: board mode draws it unselected either way
}

void SudokuScene::startFreePlay() {
  _model.load(_tier, esp_random());
  _mode = Mode::Board;
  _cursor = 0;
  _digitSel = 0;
  markDirty();  // 81 cells replaced
}

void SudokuScene::recordWin() {
  if (_daily) {
    GAME_STATS.markDailyDone(games::Game::Sudoku, _day);
  } else {
    GAME_STATS.markFreeDone(games::Game::Sudoku);
  }
  markDirty();  // chrome, ring, strip and both status lines all change at once
}

void SudokuScene::handleInput(Input& in) {
  // Every direction is ticked on every pass, in BOTH modes: HoldRepeat carries
  // its hold clock across ticks, so a direction skipped in one mode would come
  // back already past its repeat deadline and fire a phantom step.
  const uint32_t now = millis();
  bool step[kDirCount] = {};
  for (uint8_t d = 0; d < kDirCount; d++) {
    if (_rpt[d].tick(in.isPressed(kDirBtn[d]), now)) step[d] = true;
    // The tap lands on RELEASE, so a hold that auto-repeated still delivers
    // one; consumeTap() swallows it and the ring stops where it looked like it
    // stopped.
    if (in.wasPressed(kDirBtn[d]) && !_rpt[d].consumeTap()) step[d] = true;
  }

  if (_model.solved()) {
    if (in.wasPressed(Btn::Back)) {
      showGames();
    } else if (in.wasPressed(Btn::Confirm) && !_daily) {
      startFreePlay();  // NEW: another board of the same tier (the daily has exactly one)
    }
    return;
  }

  if (_mode == Mode::Board) {
    if (step[kDirLeft]) moveCursor(-1);
    if (step[kDirRight]) moveCursor(+1);
    if (step[kDirUp]) moveCursor(-games::Sudoku::kSide);
    if (step[kDirDown]) moveCursor(+games::Sudoku::kSide);
    if (in.wasLongPressed(Btn::Confirm)) {
      clearCursorCell();  // erase without a trip through the strip; mode unchanged
      return;
    }
    if (in.wasPressed(Btn::Confirm)) {
      enterDigitMode();
      return;
    }
    if (in.wasPressed(Btn::Back)) showGames();
    return;
  }

  if (step[kDirLeft]) moveSlot(-1);
  if (step[kDirRight]) moveSlot(+1);
  if (step[kDirUp]) moveSlot(-1);
  if (step[kDirDown]) moveSlot(+1);
  if (in.wasPressed(Btn::Confirm)) {
    commitSlot();
    return;
  }
  if (in.wasPressed(Btn::Back)) leaveDigitMode();
}

void SudokuScene::render(Gfx& gfx) {
  const int w = gfx.width();
  _wCache = static_cast<int16_t>(w);  // status-band rects span the panel

  // One sample of the model per frame: the chrome, the ring and both status
  // lines must agree about the same grid.
  const bool solved = _model.solved();

  // --- Header ---------------------------------------------------------------
  gfx.drawText(kFontBold, kMarginX, 8, L10N("Sudoku", "Sudoku"));
  const char* note = _daily ? L10N("daily", "del giorno") : tierLabel(_tier);
  gfx.drawText(kFontSmall, w - kMarginX - gfx.textWidth(kFontSmall, note), 12, note);
  gfx.fillRect(0, kHeaderH - 2, w, 2, true);

  // --- Rules ----------------------------------------------------------------
  gfx.drawRect(kGridX, kGridY, kGridSide, kGridSide, kBoxLine, true);
  for (int i = 1; i < games::Sudoku::kSide; i++) {
    const bool box = (i % 3) == 0;
    const int t = box ? kBoxLine : 1;
    const int off = box ? kBoxLine / 2 : 0;  // thick rules straddle the cell edge, thin ones sit on it
    gfx.fillRect(cellLeft(i) - off, kGridY, t, kGridSide, true);
    gfx.fillRect(kGridX, cellTop(i) - off, kGridSide, t, true);
  }

  // --- Digits ---------------------------------------------------------------
  for (int cell = 0; cell < games::Sudoku::kCells; cell++) {
    const uint8_t value = _model.at(cell);
    if (value == 0) continue;
    const int x = cellLeft(cell % games::Sudoku::kSide);
    const int y = cellTop(cell / games::Sudoku::kSide);
    const bool given = _model.isGiven(cell);
    const XpFont& font = given ? kFontBold : kFontRegular;
    char buf[4];
    snprintf(buf, sizeof buf, "%u", static_cast<unsigned>(value));
    gfx.drawTextCentered(font, x + kCell / 2, y + (kCell - gfx.lineHeight(font)) / 2, buf);
    // A player's wrong digit is marked, never blocked — the pack's givens
    // cannot conflict, so a mark always points at something the player did.
    if (!given && _model.conflict(cell)) gfx.fillRect(x + kMarkInset, y + kMarkY, kMarkW, 2, true);
  }

  // --- Cursor ---------------------------------------------------------------
  // Thick while the ring owns the directions, thin while the strip does; gone
  // once the grid is finished and nothing is selectable.
  if (!solved) {
    const XpRect r = cellRect(_cursor);
    gfx.drawRect(r.x, r.y, r.w, r.h, _mode == Mode::Board ? kBoxLine : 1, true);
  }

  // --- Digit strip ----------------------------------------------------------
  for (int i = 0; i < kSlotCount; i++) {
    const int x = slotLeft(i);
    const bool sel = !solved && _mode == Mode::Digit && i == _digitSel;
    if (sel) {
      gfx.fillRoundedRect(x, kStripY, kSlotW, kSlotH, kSlotRadius, true);
    } else {
      gfx.drawRoundedRect(x, kStripY, kSlotW, kSlotH, kSlotRadius, 1, true);  // clears its own interior
    }
    if (i == kClearSlot) {
      // No "erase" glyph exists in a Latin bitmap font, so the clear slot is
      // two strokes.
      gfx.drawLine(x + 13, kStripY + 16, x + 28, kStripY + 31, 2, !sel);
      gfx.drawLine(x + 28, kStripY + 16, x + 13, kStripY + 31, 2, !sel);
    } else {
      char buf[4];
      snprintf(buf, sizeof buf, "%d", i + 1);
      gfx.drawTextCentered(kFontRegular, x + kSlotW / 2, kStripY + (kSlotH - gfx.lineHeight(kFontRegular)) / 2, buf,
                           !sel);
    }
  }

  // --- Status ---------------------------------------------------------------
  char status[40];
  if (solved) {
    snprintf(status, sizeof status, "%s", L10N("Solved!", "Completato!"));
  } else {
    snprintf(status, sizeof status, L10N("%d cells left", "%d caselle libere"), _model.empty());
  }
  gfx.drawTextCentered(kFontRegular, w / 2, kStatusY, status);

  char sub[48];
  if (!_daily) {
    snprintf(sub, sizeof sub, "%s", tierLabel(_tier));
  } else if (solved) {
    snprintf(sub, sizeof sub, L10N("streak %u days", "serie %u giorni"),
             static_cast<unsigned>(GAME_STATS.record(games::Game::Sudoku).streak));
  } else {
    snprintf(sub, sizeof sub, "%s", L10N("daily puzzle", "tavola del giorno"));
  }
  gfx.drawTextCentered(kFontSmall, w / 2, kSubY, sub);
}
