#pragma once

// Lume games — sudoku: the 9x9 grid, a cell cursor and a digit strip.
//
// Why a STRIP and not typing: the device has four front buttons and two side
// buttons, no keyboard, and Input.h reports no auto-repeat. So entering a digit
// is two modes — walk a ring over the grid (Board), then pick one of ten slots,
// 1..9 plus a clear slot (Digit) — driven by the same four directions. Anything
// richer would need soft keys the bar does not have.
//
// Every rule lives in src/games/Sudoku.h; this file paints and routes buttons,
// nothing else. It never refuses a wrong digit: a conflicting entry is
// underlined and left on glass, because being wrong for a while IS the game.
//
// E-ink discipline (Scene.h refresh policy): a cursor step dirties only the two
// 58x58 cell windows it touched, a digit write the cell + strip + status band,
// and only a win asks for the full panel. A full-panel flush costs 3.2 s — one
// per keystroke would turn the grid into a slideshow.

#include <cstdint>

#include "../Scene.h"
#include "../games/HoldRepeat.h"
#include "../games/Sudoku.h"

class SudokuScene : public Scene {
 public:
  // Called by showSudoku() BEFORE switchTo: picks the board (daily index or a
  // random free-play one) and reloads any in-progress daily grid.
  void open(games::Tier tier, bool daily);
  // Sleep hook (gamesPersistDaily) and onExit: stash the unfinished daily grid
  // in NVS. Free play is disposable by design.
  void persistDaily();

  void onEnter() override;
  void onExit() override;
  void handleInput(Input& in) override;
  void render(Gfx& gfx) override;
  const char* const* softKeys() const override;
  uint8_t longPressSlots() const override;

  // Read-only inspection for host test harness
  const games::Sudoku& model() const { return _model; }
  uint8_t cursor() const { return _cursor; }
  uint8_t digitSel() const { return _digitSel; }
  bool isDigitMode() const { return _mode == Mode::Digit; }

 private:
  // Board = the ring owns the grid; Digit = the ring thins and the strip owns
  // the four directions.
  enum class Mode : uint8_t { Board, Digit };

  // One HoldRepeat per direction, ticked every pass in BOTH modes (see
  // handleInput: a direction left un-ticked comes back armed).
  static constexpr uint8_t kDirCount = 4;

  void moveCursor(int delta);
  void moveSlot(int delta);
  void enterDigitMode();
  void leaveDigitMode();
  void commitSlot();
  void clearCursorCell();
  void startFreePlay();
  void recordWin();

  games::Sudoku _model;
  games::HoldRepeat _rpt[kDirCount];
  Mode _mode = Mode::Board;
  uint8_t _cursor = 0;    // 0..80, linear so Left/Right wrap across row ends
  uint8_t _digitSel = 0;  // strip slot 0..9 (9 = clear)
  bool _daily = false;    // false whenever the clock is unset: a board with no day
  games::Tier _tier = games::kDailyTier;
  int32_t _day = 0;  // civil day serial of this daily board (0 = clock never set)
  int16_t _wCache = 0;  // panel width, for the full-width status-band dirty rect
};
