#include "Nonogram.h"

#include <cstring>

// Only TU allowed to include the pack (see Sudoku.cpp for why).
#include "pack/NonogramPack.h"

namespace games {
namespace {

const nonogram_pack::Puzzle& entry(Tier tier, uint32_t index) {
  switch (tier) {
    case Tier::Medium:
      return nonogram_pack::kMedium[index % nonogram_pack::kMediumCount];
    case Tier::Hard:
      return nonogram_pack::kHard[index % nonogram_pack::kHardCount];
    case Tier::Easy:
      break;
  }
  return nonogram_pack::kEasy[index % nonogram_pack::kEasyCount];
}

}  // namespace

uint32_t Nonogram::packCount(Tier tier) {
  switch (tier) {
    case Tier::Medium:
      return nonogram_pack::kMediumCount;
    case Tier::Hard:
      return nonogram_pack::kHardCount;
    case Tier::Easy:
      break;
  }
  return nonogram_pack::kEasyCount;
}

void Nonogram::load(Tier tier, uint32_t index) {
  const uint32_t count = packCount(tier);
  _tier = tier;
  _index = count ? (index % count) : 0u;

  const nonogram_pack::Puzzle& p = entry(tier, _index);
  _side = (p.side >= 1 && p.side <= kMaxSide) ? p.side : kMaxSide;
  _name = p.name;

  memset(_state, 0, sizeof(_state));
  memset(_target, 0, sizeof(_target));
  for (int row = 0; row < _side; ++row) {
    const uint8_t* bits = &p.rows[row * nonogram_pack::kRowBytes];
    for (int col = 0; col < _side; ++col) {
      // MSB-first inside each byte, the way the generator writes it.
      _target[idx(col, row)] = ((bits[col >> 3] >> (7 - (col & 7))) & 1u) != 0;
    }
  }
}

NonoCell Nonogram::at(int col, int row) const {
  if (!inside(col, row)) return NonoCell::Empty;
  return static_cast<NonoCell>(_state[idx(col, row)]);
}

bool Nonogram::target(int col, int row) const {
  return inside(col, row) && _target[idx(col, row)];
}

void Nonogram::setCell(int col, int row, NonoCell state) {
  if (!inside(col, row)) return;
  _state[idx(col, row)] = static_cast<uint8_t>(state);
}

void Nonogram::cycleFill(int col, int row) {
  if (!inside(col, row)) return;
  uint8_t& s = _state[idx(col, row)];
  // A Mark that turns out to be wrong is corrected with one press of the same
  // button that fills, not by clearing first.
  s = (s == static_cast<uint8_t>(NonoCell::Fill)) ? static_cast<uint8_t>(NonoCell::Empty)
                                                 : static_cast<uint8_t>(NonoCell::Fill);
}

void Nonogram::cycleMark(int col, int row) {
  if (!inside(col, row)) return;
  uint8_t& s = _state[idx(col, row)];
  s = (s == static_cast<uint8_t>(NonoCell::Mark)) ? static_cast<uint8_t>(NonoCell::Empty)
                                                 : static_cast<uint8_t>(NonoCell::Mark);
}

int Nonogram::clues(const bool* line, int n, uint8_t* out, int cap) {
  int count = 0;
  int run = 0;
  for (int i = 0; i <= n; ++i) {
    const bool on = (i < n) && line[i];
    if (on) {
      ++run;
      continue;
    }
    if (run > 0) {
      if (count < cap) out[count] = static_cast<uint8_t>(run);
      ++count;
      run = 0;
    }
  }
  // The pack guarantees <= kMaxClues groups per line; clamping here keeps a
  // hand-written or regenerated pack from writing past the caller's array.
  return (count < cap) ? count : cap;
}

int Nonogram::rowClues(int row, uint8_t out[kMaxClues]) const {
  if (row < 0 || row >= _side) return 0;
  bool line[kMaxSide];
  for (int col = 0; col < _side; ++col) line[col] = _target[idx(col, row)];
  return clues(line, _side, out, kMaxClues);
}

int Nonogram::colClues(int col, uint8_t out[kMaxClues]) const {
  if (col < 0 || col >= _side) return 0;
  bool line[kMaxSide];
  for (int row = 0; row < _side; ++row) line[row] = _target[idx(col, row)];
  return clues(line, _side, out, kMaxClues);
}

bool Nonogram::rowDone(int row) const {
  if (row < 0 || row >= _side) return false;
  for (int col = 0; col < _side; ++col) {
    const int i = idx(col, row);
    if ((_state[i] == static_cast<uint8_t>(NonoCell::Fill)) != _target[i]) return false;
  }
  return true;
}

bool Nonogram::colDone(int col) const {
  if (col < 0 || col >= _side) return false;
  for (int row = 0; row < _side; ++row) {
    const int i = idx(col, row);
    if ((_state[i] == static_cast<uint8_t>(NonoCell::Fill)) != _target[i]) return false;
  }
  return true;
}

int Nonogram::filled() const {
  int n = 0;
  for (int row = 0; row < _side; ++row) {
    for (int col = 0; col < _side; ++col) {
      if (_state[idx(col, row)] == static_cast<uint8_t>(NonoCell::Fill)) ++n;
    }
  }
  return n;
}

int Nonogram::targetCount() const {
  int n = 0;
  for (int row = 0; row < _side; ++row) {
    for (int col = 0; col < _side; ++col) {
      if (_target[idx(col, row)]) ++n;
    }
  }
  return n;
}

bool Nonogram::solved() const {
  for (int row = 0; row < _side; ++row) {
    if (!rowDone(row)) return false;
  }
  return true;
}

void Nonogram::snapshot(uint8_t out[kBlobBytes]) const {
  out[0] = static_cast<uint8_t>(_index & 0xFFu);
  out[1] = static_cast<uint8_t>((_index >> 8) & 0xFFu);
  out[2] = static_cast<uint8_t>((_index >> 16) & 0xFFu);
  out[3] = static_cast<uint8_t>((_index >> 24) & 0xFFu);
  memset(out + 4, 0, kBlobBytes - 4);
  // Fixed 57 bytes whatever the side: a blob whose length depended on the
  // puzzle would make the NVS record self-describing for no gain.
  for (int i = 0; i < kMaxCells; ++i) {
    out[4 + (i >> 2)] |= static_cast<uint8_t>((_state[i] & 0x03u) << ((i & 3) * 2));
  }
}

bool Nonogram::restore(const uint8_t in[kBlobBytes]) {
  const uint32_t index = static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) |
                         (static_cast<uint32_t>(in[2]) << 16) | (static_cast<uint32_t>(in[3]) << 24);
  if (index != _index) return false;

  uint8_t state[kMaxCells];
  for (int i = 0; i < kMaxCells; ++i) {
    const uint8_t v = static_cast<uint8_t>((in[4 + (i >> 2)] >> ((i & 3) * 2)) & 0x03u);
    if (v > static_cast<uint8_t>(NonoCell::Mark)) return false;  // 3 is not a cell state
    state[i] = v;
  }
  memcpy(_state, state, sizeof(_state));
  return true;
}

}  // namespace games
