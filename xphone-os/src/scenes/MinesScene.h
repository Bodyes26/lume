#pragma once

// Lume games — minesweeper board. The scene is a painter and an input adapter:
// the whole field lives in games::Mines (pure logic, host-tested), so nothing
// here decides anything about the game.
//
// Controls (one confirm button, so the two actions on a cell must not share an
// edge):
//   side Up/Down      = cursor row, wrapping
//   front Left/Right  = cursor column, wrapping (PREV/NEXT tabs)
//   CONFIRM tap       = reveal
//   CONFIRM long      = flag/unflag (Input fires it once at 550 ms and then
//                       swallows the release, so a hold never also reveals)
//   BACK              = games menu
// A held direction auto-repeats through games::HoldRepeat: without it, crossing
// a 12x14 field would cost twelve separate presses.
//
// E-ink discipline: a cursor move dirties the two cells it touches, a flag its
// cell plus the status band, a reveal only the bounding box of the cells the
// flood actually opened (diffed against a snapshot of the revealed mask). Only
// a win or a loss repaints the panel — that is the one moment the whole board
// changes.
//
// Daily boards are the same field all day, so a loss is final by design: the
// lost board is persisted (as-is) and reopening today shows the wreck instead
// of handing out a fresh field. Free play is disposable and never persisted.

#include <cstdint>

#include "../Scene.h"
#include "../games/GameTypes.h"
#include "../games/HoldRepeat.h"
#include "../games/Mines.h"

class MinesScene : public Scene {
 public:
  // Daily: tier is forced to games::kDailyTier and the field comes from the day
  // serial; free play seeds from esp_random(). `daily` is ignored when the clock
  // was never set (no day serial = no calendar = no streak).
  void open(games::Tier tier, bool daily);

  // Sleep/exit hook: stash an unfinished (or lost) daily board in NVS. No-op for
  // free play and for a board that was never touched.
  void persistDaily();

  void onExit() override;
  void handleInput(Input& in) override;
  void render(Gfx& gfx) override;
  const char* const* softKeys() const override;
  uint8_t longPressSlots() const override;

  // Read-only inspection for host test harness
  const games::Mines& model() const { return _model; }
  int cursorCol() const { return _col; }
  int cursorRow() const { return _row; }
  int cursor() const { return _row * _model.cols() + _col; }

 private:
  static constexpr int kMarginX = 20;
  static constexpr int kHeaderH = 46;
  static constexpr int kGridY = 62;

  bool playing() const {
    return _model.state() == games::Mines::State::Fresh || _model.state() == games::Mines::State::Playing;
  }

  // Geometry (all derived from the loaded shape; _wCache is filled by render,
  // so every rect helper returns an empty rect — i.e. full-panel dirty — before
  // the first paint).
  int cellSize() const;
  int gridX() const;
  int statusY() const;
  XpRect cellRect(int col, int row) const;  // includes the cursor ring's 3 px
  XpRect statusRect() const;

  void moveCursor(int dCol, int dRow);
  void doReveal();
  void doFlag();
  void finishIfOver();  // streak/total bookkeeping, exactly once per board

  void captureRevealed(uint8_t* mask) const;
  XpRect revealedDiffRect(const uint8_t* before) const;

  void drawCell(Gfx& gfx, int col, int row) const;
  void drawCovered(Gfx& gfx, int x, int y, int cell) const;
  void drawFlag(Gfx& gfx, int x, int y, int cell) const;
  void drawMine(Gfx& gfx, int x, int y, int cell, bool fatal) const;

  games::Mines _model;
  games::HoldRepeat _rptUp, _rptDown, _rptLeft, _rptRight;
  int _col = 0, _row = 0;
  int32_t _day = 0;      // civil day serial, 0 when the clock was never set
  bool _daily = false;   // false also when `daily` was asked for with day == 0
  bool _scored = false;  // markDailyDone/markFreeDone already ran for this board
  // The board in NVS already matches this one: keeps onExit (and the sleep hook)
  // from re-writing flash for a board nobody touched since the last save.
  bool _persisted = false;
  int8_t _boomCol = -1, _boomRow = -1;  // the mine that ended the game
  int16_t _wCache = 0, _hCache = 0;
};
