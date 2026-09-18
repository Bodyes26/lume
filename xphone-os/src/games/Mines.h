#pragma once

// Lume games — minesweeper model. The only one of the three with no flash pack:
// a field is (seed, first cell) and nothing else, which is also why an
// in-progress board persists in 48 bytes without storing where the mines are.
//
// First-reveal safety is the classic rule and it is not optional here: the field
// is laid AFTER the first reveal, with no mine in the 3x3 around it, so the game
// can never be lost on move one. Consequence for persistence: the seed alone is
// not enough, the first cell is part of the board's identity.
//
// The zero-flood is iterative (an explicit stack of at most kMaxCells entries in
// BSS): a recursive flood on the C3's 8 KB loop stack is exactly the kind of
// thing that survives testing and then overflows on the one 14x12 board that
// opens two thirds of the grid.

#include <cstdint>

#include "GameTypes.h"

namespace games {

class Mines {
 public:
  static constexpr int kMaxCols = 12;
  static constexpr int kMaxRows = 14;
  static constexpr int kMaxCells = kMaxCols * kMaxRows;  // 168
  static constexpr int kMaskBytes = (kMaxCells + 7) / 8;  // 21
  // seed (LE) + first cell + state + revealed mask + flag mask.
  static constexpr int kBlobBytes = 4 + 1 + 1 + 2 * kMaskBytes;  // 48

  struct Shape {
    uint8_t cols, rows, mines;
  };
  // Sized so the widest board still gets a >=40 px cell on the 528 px panel and
  // stays above the soft-key bar: 8x10, 10x12, 12x14.
  static Shape shapeFor(Tier tier);

  enum class State : uint8_t { Fresh = 0, Playing = 1, Won = 2, Lost = 3 };

  // Fresh board: geometry from the tier, mines not placed yet.
  void load(Tier tier, uint32_t seed);

  Tier tier() const { return _tier; }
  uint32_t seed() const { return _seed; }
  int cols() const { return _cols; }
  int rows() const { return _rows; }
  int mines() const { return _mines; }
  State state() const { return _state; }

  bool revealed(int col, int row) const;
  bool flagged(int col, int row) const;
  // Meaningful once the field exists (State != Fresh); false before that.
  bool mine(int col, int row) const;
  // 0..8 neighbouring mines; 0 before the field exists.
  int adjacent(int col, int row) const;

  int flagsPlaced() const;
  int minesLeft() const { return _mines - flagsPlaced(); }
  int revealedCount() const;

  // Reveal, flooding neighbours while the count is 0. The first reveal lays the
  // field around this cell. A flagged cell is protected (no-op) — on a device
  // with one confirm button, an accidental reveal of a cell you deliberately
  // flagged is the worst possible outcome. Revealing a mine sets State::Lost and
  // stops the game; every further reveal/flag is then a no-op.
  void reveal(int col, int row);
  void toggleFlag(int col, int row);

  void snapshot(uint8_t out[kBlobBytes]) const;
  // Rebuilds the field from the stored (seed, first cell) and replays the masks;
  // refuses a blob whose geometry does not match the loaded tier.
  bool restore(const uint8_t in[kBlobBytes]);

 private:
  int idx(int col, int row) const { return row * _cols + col; }
  bool inside(int col, int row) const { return col >= 0 && row >= 0 && col < _cols && row < _rows; }
  static bool getBit(const uint8_t* mask, int i) { return (mask[i >> 3] >> (i & 7)) & 1u; }
  static void setBit(uint8_t* mask, int i, bool v);
  void layField(int safeCell);
  void floodFrom(int cell);
  void checkWin();

  uint8_t _mineMask[kMaskBytes] = {};
  uint8_t _revealMask[kMaskBytes] = {};
  uint8_t _flagMask[kMaskBytes] = {};
  uint8_t _stack[kMaxCells] = {};  // flood worklist, BSS not call stack
  uint32_t _seed = 1;
  int _cols = 8, _rows = 10, _mines = 10;
  int _firstCell = -1;
  Tier _tier = Tier::Easy;
  State _state = State::Fresh;
};

}  // namespace games
