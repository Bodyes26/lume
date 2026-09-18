#pragma once

// Lume games — nonogram (picross) model: pack decode, clue derivation, three
// cell states, solved check, snapshot.
//
// The pictures live in src/games/pack/NonogramPack.h, emitted by
// tools/lume-games/gen_nonogram_pack.py, which enforces the four properties a
// nonogram needs to be worth solving on a device with no undo:
//   1. line-solvable — a fixpoint of per-line enumeration finishes the grid, so
//      no guessing is ever required;
//   2. unique solution;
//   3. at most kMaxClues groups per line, so the clue gutters fit the panel;
//   4. an ink ratio that reads as a picture rather than a smear.
// Generating that offline on the Mac is the whole reason this is a build-time
// pack and not a BLE payload: the phone would have to ship the same solver to
// prove the same properties, and the packs cost 34 bytes per picture.
//
// Cell semantics: Fill is a claim about the picture, Mark is the player's "this
// one is definitely blank" bookkeeping. solved() ignores Mark entirely — a grid
// with every Fill right is finished even if the player never marked a blank.

#include <cstdint>

#include "GameTypes.h"

namespace games {

enum class NonoCell : uint8_t { Empty = 0, Fill = 1, Mark = 2 };

class Nonogram {
 public:
  static constexpr int kMaxSide = 15;
  static constexpr int kMaxCells = kMaxSide * kMaxSide;  // 225
  static constexpr int kMaxClues = 4;
  // puzzle index (LE) + 225 cells at 2 bits each.
  static constexpr int kBlobBytes = 4 + 57;

  static uint32_t packCount(Tier tier);

  void load(Tier tier, uint32_t index);

  Tier tier() const { return _tier; }
  uint32_t index() const { return _index; }
  int side() const { return _side; }
  // Localized picture name, revealed when the grid is finished ("gatto"/"cat").
  const char* name() const { return _name; }

  NonoCell at(int col, int row) const;
  bool target(int col, int row) const;  // the solution bit

  void setCell(int col, int row, NonoCell state);
  void cycleFill(int col, int row);  // Empty/Mark -> Fill -> Empty
  void cycleMark(int col, int row);  // Empty/Fill -> Mark -> Empty

  // Clue groups for a line, written into `out`; returns the group count (0 for
  // an entirely blank line, which the scene draws as "0").
  int rowClues(int row, uint8_t out[kMaxClues]) const;
  int colClues(int col, uint8_t out[kMaxClues]) const;
  // Every Fill in the line is right AND no target cell in it is missing — the
  // cue the scene uses to tick off a finished clue.
  bool rowDone(int row) const;
  bool colDone(int col) const;

  int filled() const;
  int targetCount() const;
  bool solved() const;

  void snapshot(uint8_t out[kBlobBytes]) const;
  bool restore(const uint8_t in[kBlobBytes]);

 private:
  int idx(int col, int row) const { return row * _side + col; }
  bool inside(int col, int row) const { return col >= 0 && row >= 0 && col < _side && row < _side; }
  static int clues(const bool* line, int n, uint8_t* out, int cap);

  uint8_t _state[kMaxCells] = {};   // NonoCell per cell
  bool _target[kMaxCells] = {};     // decoded picture
  const char* _name = "";
  int _side = 10;
  Tier _tier = Tier::Easy;
  uint32_t _index = 0;
};

}  // namespace games
