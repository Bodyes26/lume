#pragma once

// Lume games — nonogram (picross) board.
//
// The scene is a painter and input adapter; all line-solving rules, clue
// derivations and picture states live in games::Nonogram (pure logic,
// host-tested).
//
// Controls:
//   side Up/Down      = row cursor, wrapping
//   front Left/Right  = column cursor, wrapping (PREV/NEXT tabs)
//   CONFIRM tap       = cycle Fill / Empty (or Empty / Mark if previously Mark)
//   CONFIRM long      = cycle Mark (x) / Empty (Input fires it once at 550 ms
//                       and consumes the release, so a hold never also fills)
//   BACK              = games menu
// Direction buttons auto-repeat through games::HoldRepeat for comfortable
// traverse across 10..15 wide grids.
//
// E-ink discipline: a cursor step dirties the two cells it touched (plus the
// cursor ring's 3 px), a cell fill/mark dirties its cell + the two 5 px clue
// indicators + the status band, and only a win repaints the full panel.

#include <cstdint>

#include "../Scene.h"
#include "../games/GameTypes.h"
#include "../games/HoldRepeat.h"
#include "../games/Nonogram.h"

class NonogramScene : public Scene {
 public:
  void open(games::Tier tier, bool daily);
  void persistDaily();

  void onExit() override;
  void handleInput(Input& in) override;
  void render(Gfx& gfx) override;
  const char* const* softKeys() const override;
  uint8_t longPressSlots() const override;

  // Read-only inspection for host test harness
  const games::Nonogram& model() const { return _model; }
  int cursorCol() const { return _col; }
  int cursorRow() const { return _row; }
  int cursor() const { return _row * _model.side() + _col; }

 private:
  static constexpr int kHeaderH = 46;
  static constexpr int kTopGutterH = 88;
  static constexpr int kLeftGutterW = 96;
  static constexpr int kGridY = 54 + kTopGutterH;  // 142

  int cellSize() const;
  int gridX() const;
  int blockW() const;
  int statusY() const;

  XpRect cellRect(int col, int row) const;
  XpRect rowIndicatorRect(int row) const;
  XpRect colIndicatorRect(int col) const;
  XpRect statusRect() const;

  void moveCursor(int dCol, int dRow);
  void toggleFill();
  void toggleMark();
  void checkFinish();

  void drawGrid(Gfx& gfx, int gx, int gy, int cs, int s) const;
  void drawClues(Gfx& gfx, int gx, int gy, int cs, int s) const;
  void drawCells(Gfx& gfx, int gx, int gy, int cs, int s) const;
  void drawIndicators(Gfx& gfx, int x0, int gx, int gy, int cs, int s) const;

  games::Nonogram _model;
  games::HoldRepeat _rptUp, _rptDown, _rptLeft, _rptRight;
  int _col = 0, _row = 0;
  int32_t _day = 0;
  bool _daily = false;
  bool _scored = false;
  bool _persisted = false;
  int16_t _wCache = 0, _hCache = 0;
};
