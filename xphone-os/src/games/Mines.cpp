#include "Mines.h"

#include <cstring>

namespace games {

Mines::Shape Mines::shapeFor(Tier tier) {
  switch (tier) {
    case Tier::Medium:
      return Shape{10, 12, 20};
    case Tier::Hard:
      return Shape{12, 14, 35};
    case Tier::Easy:
      break;
  }
  return Shape{8, 10, 10};
}

void Mines::setBit(uint8_t* mask, int i, bool v) {
  const uint8_t m = static_cast<uint8_t>(1u << (i & 7));
  if (v) {
    mask[i >> 3] = static_cast<uint8_t>(mask[i >> 3] | m);
  } else {
    mask[i >> 3] = static_cast<uint8_t>(mask[i >> 3] & static_cast<uint8_t>(~m));
  }
}

void Mines::load(Tier tier, uint32_t seed) {
  const Shape s = shapeFor(tier);
  _tier = tier;
  _cols = s.cols;
  _rows = s.rows;
  _mines = s.mines;
  _seed = seed;
  _firstCell = -1;
  _state = State::Fresh;
  memset(_mineMask, 0, sizeof(_mineMask));
  memset(_revealMask, 0, sizeof(_revealMask));
  memset(_flagMask, 0, sizeof(_flagMask));
}

bool Mines::revealed(int col, int row) const {
  return inside(col, row) && getBit(_revealMask, idx(col, row));
}

bool Mines::flagged(int col, int row) const {
  return inside(col, row) && getBit(_flagMask, idx(col, row));
}

bool Mines::mine(int col, int row) const {
  if (_state == State::Fresh || !inside(col, row)) return false;
  return getBit(_mineMask, idx(col, row));
}

int Mines::adjacent(int col, int row) const {
  if (_state == State::Fresh || !inside(col, row)) return 0;
  int n = 0;
  for (int dr = -1; dr <= 1; ++dr) {
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      const int c = col + dc;
      const int r = row + dr;
      if (inside(c, r) && getBit(_mineMask, idx(c, r))) ++n;
    }
  }
  return n;
}

int Mines::flagsPlaced() const {
  int n = 0;
  for (int i = 0, cells = _cols * _rows; i < cells; ++i) {
    if (getBit(_flagMask, i)) ++n;
  }
  return n;
}

int Mines::revealedCount() const {
  int n = 0;
  for (int i = 0, cells = _cols * _rows; i < cells; ++i) {
    if (getBit(_revealMask, i)) ++n;
  }
  return n;
}

void Mines::layField(int safeCell) {
  memset(_mineMask, 0, sizeof(_mineMask));
  const int cells = _cols * _rows;
  const int safeCol = safeCell % _cols;
  const int safeRow = safeCell / _cols;

  Rng rng(_seed);
  int placed = 0;
  // Rejection sampling terminates because every tier keeps mines below
  // cells - 9 (the forbidden 3x3), so free cells always remain.
  while (placed < _mines) {
    const int cell = static_cast<int>(rng.below(static_cast<uint32_t>(cells)));
    if (getBit(_mineMask, cell)) continue;
    const int dc = (cell % _cols) - safeCol;
    const int dr = (cell / _cols) - safeRow;
    if (dc >= -1 && dc <= 1 && dr >= -1 && dr <= 1) continue;  // first click opens a zero
    setBit(_mineMask, cell, true);
    ++placed;
  }
}

void Mines::floodFrom(int cell) {
  // Cells are marked revealed when they are PUSHED, never when popped: that is
  // what bounds _stack at kMaxCells entries (each cell enters it at most once)
  // and keeps the flood off the C3's 8 KB call stack.
  if (getBit(_revealMask, cell) || getBit(_flagMask, cell)) return;
  setBit(_revealMask, cell, true);
  int sp = 0;
  _stack[sp++] = static_cast<uint8_t>(cell);

  while (sp > 0) {
    const int c = _stack[--sp];
    const int col = c % _cols;
    const int row = c / _cols;
    if (adjacent(col, row) != 0) continue;  // numbered cells are the flood's border
    for (int dr = -1; dr <= 1; ++dr) {
      for (int dc = -1; dc <= 1; ++dc) {
        if (dr == 0 && dc == 0) continue;
        const int nc = col + dc;
        const int nr = row + dr;
        if (!inside(nc, nr)) continue;
        const int n = idx(nc, nr);
        if (getBit(_revealMask, n) || getBit(_flagMask, n)) continue;
        setBit(_revealMask, n, true);
        _stack[sp++] = static_cast<uint8_t>(n);
      }
    }
  }
}

void Mines::checkWin() {
  if (_state != State::Playing) return;
  if (revealedCount() == _cols * _rows - _mines) _state = State::Won;
}

void Mines::reveal(int col, int row) {
  if (!inside(col, row)) return;
  if (_state == State::Won || _state == State::Lost) return;
  const int cell = idx(col, row);
  if (getBit(_flagMask, cell)) return;  // a deliberate flag outranks a stray confirm
  if (getBit(_revealMask, cell)) return;

  if (_state == State::Fresh) {
    _firstCell = cell;
    layField(cell);
    _state = State::Playing;
  }

  if (getBit(_mineMask, cell)) {
    setBit(_revealMask, cell, true);  // the scene draws the mine that ended it
    _state = State::Lost;
    return;
  }

  if (adjacent(col, row) == 0) {
    floodFrom(cell);
  } else {
    setBit(_revealMask, cell, true);
  }
  checkWin();
}

void Mines::toggleFlag(int col, int row) {
  if (!inside(col, row)) return;
  if (_state == State::Won || _state == State::Lost) return;
  const int cell = idx(col, row);
  if (getBit(_revealMask, cell)) return;
  setBit(_flagMask, cell, !getBit(_flagMask, cell));
}

void Mines::snapshot(uint8_t out[kBlobBytes]) const {
  out[0] = static_cast<uint8_t>(_seed & 0xFFu);
  out[1] = static_cast<uint8_t>((_seed >> 8) & 0xFFu);
  out[2] = static_cast<uint8_t>((_seed >> 16) & 0xFFu);
  out[3] = static_cast<uint8_t>((_seed >> 24) & 0xFFu);
  out[4] = (_firstCell >= 0) ? static_cast<uint8_t>(_firstCell) : 0xFFu;
  out[5] = static_cast<uint8_t>(_state);
  memcpy(out + 6, _revealMask, kMaskBytes);
  memcpy(out + 6 + kMaskBytes, _flagMask, kMaskBytes);
}

bool Mines::restore(const uint8_t in[kBlobBytes]) {
  const uint32_t seed = static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) |
                        (static_cast<uint32_t>(in[2]) << 16) | (static_cast<uint32_t>(in[3]) << 24);
  const uint8_t rawFirst = in[4];
  const uint8_t rawState = in[5];
  if (rawState > static_cast<uint8_t>(State::Lost)) return false;
  const State state = static_cast<State>(rawState);

  const int cells = _cols * _rows;
  if (state == State::Fresh) {
    if (rawFirst != 0xFFu) return false;
  } else if (rawFirst >= cells) {
    return false;
  }

  // The masks carry the geometry: a bit set outside this tier's board means the
  // blob was written by a different shape (tier switched, or an older pack of
  // sizes), and replaying it would put mines under revealed cells.
  for (int i = cells; i < kMaxCells; ++i) {
    if (getBit(in + 6, i) || getBit(in + 6 + kMaskBytes, i)) return false;
  }
  for (int i = 0; i < cells; ++i) {
    if (getBit(in + 6, i) && getBit(in + 6 + kMaskBytes, i)) return false;  // revealed AND flagged
  }
  if (state == State::Fresh) {
    for (int i = 0; i < cells; ++i) {
      if (getBit(in + 6, i)) return false;  // nothing can be open before the field exists
    }
  }

  _seed = seed;
  _firstCell = (rawFirst == 0xFFu) ? -1 : static_cast<int>(rawFirst);
  memset(_mineMask, 0, sizeof(_mineMask));
  if (state != State::Fresh) layField(_firstCell);
  memcpy(_revealMask, in + 6, kMaskBytes);
  memcpy(_flagMask, in + 6 + kMaskBytes, kMaskBytes);
  _state = state;
  return true;
}

}  // namespace games
