#pragma once

// Lume games — sudoku model: pack decode, edit rules, conflict marks, solved
// check, and a 45-byte board snapshot so the daily grid survives deep sleep.
//
// The boards are NOT generated on the device. tools/lume-games/gen_sudoku_pack.py
// emits src/games/pack/SudokuPack.h with three tiers of puzzles that were each
// verified unique-solution and classified by the techniques a human needs, so
// "facile" really is singles-only. Cost: 41 bytes per puzzle in flash — the
// cheapest thing this firmware stores, against ~4 MB free in the OTA slot.
//
// The model never enforces correctness: a wrong digit is allowed and merely
// marked (conflict()), because being wrong for a while IS the game. Only
// solved() is authoritative.

#include <cstdint>

#include "GameTypes.h"

namespace games {

class Sudoku {
 public:
  static constexpr int kSide = 9;
  static constexpr int kCells = 81;
  // 81 cells, one nibble each (0 = empty, 1..9), even cell in the HIGH nibble —
  // the same packing the flash pack uses, so snapshot/restore and the generator
  // speak one format.
  static constexpr int kBlobBytes = 4 + 41;  // puzzle index (LE) + 41 packed cells

  static uint32_t packCount(Tier tier);

  // Load pack entry `index % packCount(tier)`; clears every player edit.
  void load(Tier tier, uint32_t index);

  Tier tier() const { return _tier; }
  uint32_t index() const { return _index; }

  uint8_t at(int cell) const { return (cell < 0 || cell >= kCells) ? 0u : _cells[cell]; }
  bool isGiven(int cell) const;
  // 1..9 writes, 0 clears. Givens are immutable: returns false and changes
  // nothing (the scene uses the false to keep the panel still).
  bool set(int cell, uint8_t value);
  // True when this cell's digit repeats in its row, column or box. Given cells
  // can never conflict with each other (the pack is valid), so a mark always
  // points at something the player did.
  bool conflict(int cell) const;
  int filled() const;
  int empty() const { return kCells - filled(); }
  // All 81 filled with no conflict. The pack guarantees a unique solution, so a
  // complete consistent grid IS that solution — no stored answer key needed.
  bool solved() const;

  // Deep-sleep persistence for the DAILY board (free play is disposable).
  void snapshot(uint8_t out[kBlobBytes]) const;
  // Accepts only a blob from this same pack entry whose given cells still match
  // the loaded puzzle; anything else (pack regenerated, tier switched, blob from
  // an older firmware) is refused and the caller keeps the fresh board.
  bool restore(const uint8_t in[kBlobBytes]);

 private:
  static int rowOf(int cell) { return cell / kSide; }
  static int colOf(int cell) { return cell % kSide; }

  uint8_t _cells[kCells] = {};   // current grid, 0 = empty
  uint8_t _givens[kCells] = {};  // pack values, 0 = free cell
  Tier _tier = Tier::Medium;
  uint32_t _index = 0;
};

}  // namespace games
