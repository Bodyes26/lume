#include "Sudoku.h"

#include <cstring>

// The only translation unit allowed to see the pack: those constexpr arrays are
// emitted into every TU that includes the header, so a second include would
// duplicate ~30 KB of flash for nothing.
#include "pack/SudokuPack.h"

namespace games {
namespace {

// Nibble packing shared with the generator: even cell in the HIGH nibble.
uint8_t nibbleAt(const uint8_t* packed, int cell) {
  const uint8_t byte = packed[cell >> 1];
  return (cell & 1) ? (byte & 0x0Fu) : static_cast<uint8_t>(byte >> 4);
}

void writeNibble(uint8_t* packed, int cell, uint8_t value) {
  uint8_t& byte = packed[cell >> 1];
  if (cell & 1) {
    byte = static_cast<uint8_t>((byte & 0xF0u) | (value & 0x0Fu));
  } else {
    byte = static_cast<uint8_t>((byte & 0x0Fu) | static_cast<uint8_t>((value & 0x0Fu) << 4));
  }
}

const uint8_t* entry(Tier tier, uint32_t index) {
  switch (tier) {
    case Tier::Easy:
      return sudoku_pack::kEasy[index % sudoku_pack::kEasyCount];
    case Tier::Hard:
      return sudoku_pack::kHard[index % sudoku_pack::kHardCount];
    case Tier::Medium:
      break;
  }
  return sudoku_pack::kMedium[index % sudoku_pack::kMediumCount];
}

}  // namespace

uint32_t Sudoku::packCount(Tier tier) {
  switch (tier) {
    case Tier::Easy:
      return sudoku_pack::kEasyCount;
    case Tier::Hard:
      return sudoku_pack::kHardCount;
    case Tier::Medium:
      break;
  }
  return sudoku_pack::kMediumCount;
}

void Sudoku::load(Tier tier, uint32_t index) {
  const uint32_t count = packCount(tier);
  _tier = tier;
  _index = count ? (index % count) : 0u;

  const uint8_t* packed = entry(tier, _index);
  for (int cell = 0; cell < kCells; ++cell) {
    const uint8_t v = nibbleAt(packed, cell);
    _givens[cell] = (v <= 9) ? v : 0u;  // a corrupt nibble becomes a free cell, never a bogus digit
    _cells[cell] = _givens[cell];
  }
}

bool Sudoku::isGiven(int cell) const {
  return cell >= 0 && cell < kCells && _givens[cell] != 0;
}

bool Sudoku::set(int cell, uint8_t value) {
  if (cell < 0 || cell >= kCells || value > 9) return false;
  if (_givens[cell] != 0) return false;
  _cells[cell] = value;
  return true;
}

bool Sudoku::conflict(int cell) const {
  if (cell < 0 || cell >= kCells) return false;
  const uint8_t v = _cells[cell];
  if (v == 0) return false;
  // A given is never marked: the pack is consistent, so the wrong digit of any
  // clashing pair is always the player's, and marking the clue instead would
  // read as "the puzzle is broken".
  if (_givens[cell] != 0) return false;

  const int row = rowOf(cell);
  const int col = colOf(cell);
  for (int i = 0; i < kSide; ++i) {
    const int rowCell = row * kSide + i;
    if (rowCell != cell && _cells[rowCell] == v) return true;
    const int colCell = i * kSide + col;
    if (colCell != cell && _cells[colCell] == v) return true;
  }

  const int boxRow = (row / 3) * 3;
  const int boxCol = (col / 3) * 3;
  for (int r = boxRow; r < boxRow + 3; ++r) {
    for (int c = boxCol; c < boxCol + 3; ++c) {
      const int boxCell = r * kSide + c;
      if (boxCell != cell && _cells[boxCell] == v) return true;
    }
  }
  return false;
}

int Sudoku::filled() const {
  int n = 0;
  for (int cell = 0; cell < kCells; ++cell) {
    if (_cells[cell] != 0) ++n;
  }
  return n;
}

bool Sudoku::solved() const {
  // Checked directly against the three constraints rather than through
  // conflict(), which deliberately stays silent on givens.
  for (int i = 0; i < kSide; ++i) {
    uint16_t rowSeen = 0, colSeen = 0, boxSeen = 0;
    const int boxRow = (i / 3) * 3;
    const int boxCol = (i % 3) * 3;
    for (int j = 0; j < kSide; ++j) {
      const uint8_t rowVal = _cells[i * kSide + j];
      const uint8_t colVal = _cells[j * kSide + i];
      const uint8_t boxVal = _cells[(boxRow + j / 3) * kSide + boxCol + j % 3];
      if (rowVal == 0 || colVal == 0 || boxVal == 0) return false;
      const uint16_t rowBit = static_cast<uint16_t>(1u << rowVal);
      const uint16_t colBit = static_cast<uint16_t>(1u << colVal);
      const uint16_t boxBit = static_cast<uint16_t>(1u << boxVal);
      if ((rowSeen & rowBit) || (colSeen & colBit) || (boxSeen & boxBit)) return false;
      rowSeen |= rowBit;
      colSeen |= colBit;
      boxSeen |= boxBit;
    }
  }
  return true;
}

void Sudoku::snapshot(uint8_t out[kBlobBytes]) const {
  out[0] = static_cast<uint8_t>(_index & 0xFFu);
  out[1] = static_cast<uint8_t>((_index >> 8) & 0xFFu);
  out[2] = static_cast<uint8_t>((_index >> 16) & 0xFFu);
  out[3] = static_cast<uint8_t>((_index >> 24) & 0xFFu);
  memset(out + 4, 0, kBlobBytes - 4);
  for (int cell = 0; cell < kCells; ++cell) {
    writeNibble(out + 4, cell, _cells[cell]);
  }
}

bool Sudoku::restore(const uint8_t in[kBlobBytes]) {
  const uint32_t index = static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) |
                         (static_cast<uint32_t>(in[2]) << 16) | (static_cast<uint32_t>(in[3]) << 24);
  if (index != _index) return false;

  uint8_t cells[kCells];
  for (int cell = 0; cell < kCells; ++cell) {
    const uint8_t v = nibbleAt(in + 4, cell);
    if (v > 9) return false;
    // The clues are the puzzle's fingerprint: if the pack was regenerated under
    // the player's feet, entry N is a different board and the edits are noise.
    if (_givens[cell] != 0 && v != _givens[cell]) return false;
    cells[cell] = v;
  }
  memcpy(_cells, cells, sizeof(_cells));
  return true;
}

}  // namespace games
