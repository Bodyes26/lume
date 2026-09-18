#include "TrailVM.h"

namespace trail {

TrailVM::TrailVM() {
  // Default cell characters: 0='.', 1='#', 2='*', 3='+', etc.
  for (int i = 0; i < kMaxCellTypes; ++i) {
    _cellChars[i] = (i == 0) ? '.' : ('0' + i);
    _cellStyles[i] = 1; // black by default
  }
}

void TrailVM::load(const uint8_t* setupCode, uint16_t setupLen,
                   const uint8_t* const keyCodes[6], const uint16_t keyLens[6],
                   const char* const* stringTable, uint8_t stringCount,
                   uint32_t rngSeed) {
  _setupCode = setupCode;
  _setupLen  = setupLen;
  for (int i = 0; i < 6; ++i) {
    _keyCodes[i] = keyCodes ? keyCodes[i] : nullptr;
    _keyLens[i]  = keyLens ? keyLens[i] : 0;
  }
  _strings     = stringTable;
  _stringCount = stringCount;
  _rng         = rngSeed ? rngSeed : 0x9E3779B9u;

  _state = VMState::Fresh;
  _score = 0;
  _sp = 0;
  _csp = 0;
  _curX = 0;
  _curY = 0;
  _gridW = 8;
  _gridH = 6;
  _textId = -1;
  _titleId = -1;
  _barVarId = -1;
  _flash = false;

  memset(_vars, 0, sizeof(_vars));
  memset(_grid, 0, sizeof(_grid));
  memset(_stack, 0, sizeof(_stack));
}

void TrailVM::start() {
  _state = VMState::Playing;
  _score = 0;
  _sp = 0;
  _csp = 0;
  if (_setupCode && _setupLen > 0) {
    execute(_setupCode, _setupLen);
  }
}

void TrailVM::handleKey(Key key) {
  if (_state != VMState::Playing) return;
  int idx = static_cast<int>(key);
  if (idx < 0 || idx >= 6) return;
  if (_keyCodes[idx] && _keyLens[idx] > 0) {
    execute(_keyCodes[idx], _keyLens[idx]);
  }
}

uint8_t TrailVM::cell(int x, int y) const {
  if (x < 0 || x >= _gridW || y < 0 || y >= _gridH) return 0;
  return _grid[y][x];
}

const char* TrailVM::string(int id) const {
  if (!_strings || id < 0 || id >= _stringCount) return "";
  return _strings[id] ? _strings[id] : "";
}

void TrailVM::push(int16_t val) {
  if (_sp < kMaxStack) {
    _stack[_sp++] = val;
  }
}

int16_t TrailVM::pop() {
  if (_sp > 0) {
    return _stack[--_sp];
  }
  return 0;
}

int16_t TrailVM::peek() const {
  if (_sp > 0) {
    return _stack[_sp - 1];
  }
  return 0;
}

uint32_t TrailVM::nextRng() {
  uint32_t x = _rng;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  _rng = x;
  return x;
}

uint32_t TrailVM::rngBelow(uint32_t bound) {
  if (bound == 0) return 0;
  return nextRng() % bound;
}

int TrailVM::countAdj8(int x, int y) const {
  int count = 0;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) continue;
      int nx = x + dx;
      int ny = y + dy;
      if (nx >= 0 && nx < _gridW && ny >= 0 && ny < _gridH) {
        if (_grid[ny][nx] != 0) ++count;
      }
    }
  }
  return count;
}

int TrailVM::countAdj4(int x, int y) const {
  int count = 0;
  constexpr int dxs[] = {0, 0, -1, 1};
  constexpr int dys[] = {-1, 1, 0, 0};
  for (int i = 0; i < 4; ++i) {
    int nx = x + dxs[i];
    int ny = y + dys[i];
    if (nx >= 0 && nx < _gridW && ny >= 0 && ny < _gridH) {
      if (_grid[ny][nx] != 0) ++count;
    }
  }
  return count;
}

void TrailVM::floodFill(int startX, int startY, uint8_t oldVal, uint8_t newVal) {
  if (startX < 0 || startX >= _gridW || startY < 0 || startY >= _gridH) return;
  if (oldVal == newVal) return;
  if (_grid[startY][startX] != oldVal) return;

  // Explicit queue using a stack on BSS / static
  uint8_t qx[kMaxGridCells];
  uint8_t qy[kMaxGridCells];
  int qHead = 0, qTail = 0;

  qx[qTail] = static_cast<uint8_t>(startX);
  qy[qTail] = static_cast<uint8_t>(startY);
  qTail++;
  _grid[startY][startX] = newVal;

  while (qHead < qTail) {
    int cx = qx[qHead];
    int cy = qy[qHead];
    qHead++;

    constexpr int dxs[] = {0, 0, -1, 1};
    constexpr int dys[] = {-1, 1, 0, 0};
    for (int i = 0; i < 4; ++i) {
      int nx = cx + dxs[i];
      int ny = cy + dys[i];
      if (nx >= 0 && nx < _gridW && ny >= 0 && ny < _gridH) {
        if (_grid[ny][nx] == oldVal && qTail < kMaxGridCells) {
          _grid[ny][nx] = newVal;
          qx[qTail] = static_cast<uint8_t>(nx);
          qy[qTail] = static_cast<uint8_t>(ny);
          qTail++;
        }
      }
    }
  }
}

void TrailVM::execute(const uint8_t* code, uint16_t len) {
  if (!code || len == 0) return;

  uint16_t pc = 0;
  int instructions = 0;

  while (pc < len && instructions < kMaxInstructions) {
    instructions++;
    uint8_t opcode = code[pc++];

    switch (opcode) {
      // ── Stack ─────────────────────────────────────────────────────────────
      case op::PUSH: {
        if (pc + 1 >= len) return;
        int16_t imm = static_cast<int16_t>(code[pc] | (code[pc + 1] << 8));
        pc += 2;
        push(imm);
        break;
      }
      case op::POP: {
        pop();
        break;
      }
      case op::DUP: {
        push(peek());
        break;
      }
      case op::SWAP: {
        if (_sp >= 2) {
          int16_t a = _stack[_sp - 1];
          _stack[_sp - 1] = _stack[_sp - 2];
          _stack[_sp - 2] = a;
        }
        break;
      }
      case op::OVER: {
        if (_sp >= 2) {
          push(_stack[_sp - 2]);
        }
        break;
      }

      // ── Arithmetic ────────────────────────────────────────────────────────
      case op::ADD: {
        int16_t b = pop();
        int16_t a = pop();
        push(static_cast<int16_t>(a + b));
        break;
      }
      case op::SUB: {
        int16_t b = pop();
        int16_t a = pop();
        push(static_cast<int16_t>(a - b));
        break;
      }
      case op::MUL: {
        int16_t b = pop();
        int16_t a = pop();
        push(static_cast<int16_t>(a * b));
        break;
      }
      case op::DIV: {
        int16_t b = pop();
        int16_t a = pop();
        push(b != 0 ? static_cast<int16_t>(a / b) : 0);
        break;
      }
      case op::MOD: {
        int16_t b = pop();
        int16_t a = pop();
        push(b != 0 ? static_cast<int16_t>(a % b) : 0);
        break;
      }
      case op::NEG: {
        int16_t a = pop();
        push(static_cast<int16_t>(-a));
        break;
      }
      case op::ABS: {
        int16_t a = pop();
        push(a < 0 ? static_cast<int16_t>(-a) : a);
        break;
      }
      case op::MIN: {
        int16_t b = pop();
        int16_t a = pop();
        push(a < b ? a : b);
        break;
      }
      case op::MAX: {
        int16_t b = pop();
        int16_t a = pop();
        push(a > b ? a : b);
        break;
      }
      case op::CLAMP: {
        int16_t hi = pop();
        int16_t lo = pop();
        int16_t val = pop();
        if (val < lo) val = lo;
        if (val > hi) val = hi;
        push(val);
        break;
      }

      // ── Comparison ────────────────────────────────────────────────────────
      case op::EQ: {
        int16_t b = pop();
        int16_t a = pop();
        push(a == b ? 1 : 0);
        break;
      }
      case op::NE: {
        int16_t b = pop();
        int16_t a = pop();
        push(a != b ? 1 : 0);
        break;
      }
      case op::LT: {
        int16_t b = pop();
        int16_t a = pop();
        push(a < b ? 1 : 0);
        break;
      }
      case op::GT: {
        int16_t b = pop();
        int16_t a = pop();
        push(a > b ? 1 : 0);
        break;
      }
      case op::LE: {
        int16_t b = pop();
        int16_t a = pop();
        push(a <= b ? 1 : 0);
        break;
      }
      case op::GE: {
        int16_t b = pop();
        int16_t a = pop();
        push(a >= b ? 1 : 0);
        break;
      }

      // ── Logic ─────────────────────────────────────────────────────────────
      case op::AND: {
        int16_t b = pop();
        int16_t a = pop();
        push((a != 0 && b != 0) ? 1 : 0);
        break;
      }
      case op::OR: {
        int16_t b = pop();
        int16_t a = pop();
        push((a != 0 || b != 0) ? 1 : 0);
        break;
      }
      case op::NOT: {
        int16_t a = pop();
        push(a == 0 ? 1 : 0);
        break;
      }

      // ── Flow control ──────────────────────────────────────────────────────
      case op::JMP: {
        if (pc + 1 >= len) return;
        int16_t off = static_cast<int16_t>(code[pc] | (code[pc + 1] << 8));
        pc += 2;
        int32_t target = static_cast<int32_t>(pc) + off;
        if (target >= 0 && target <= len) {
          pc = static_cast<uint16_t>(target);
        }
        break;
      }
      case op::JZ: {
        if (pc + 1 >= len) return;
        int16_t off = static_cast<int16_t>(code[pc] | (code[pc + 1] << 8));
        pc += 2;
        int16_t cond = pop();
        if (cond == 0) {
          int32_t target = static_cast<int32_t>(pc) + off;
          if (target >= 0 && target <= len) {
            pc = static_cast<uint16_t>(target);
          }
        }
        break;
      }
      case op::JNZ: {
        if (pc + 1 >= len) return;
        int16_t off = static_cast<int16_t>(code[pc] | (code[pc + 1] << 8));
        pc += 2;
        int16_t cond = pop();
        if (cond != 0) {
          int32_t target = static_cast<int32_t>(pc) + off;
          if (target >= 0 && target <= len) {
            pc = static_cast<uint16_t>(target);
          }
        }
        break;
      }
      case op::CALL: {
        if (pc + 1 >= len) return;
        int16_t off = static_cast<int16_t>(code[pc] | (code[pc + 1] << 8));
        pc += 2;
        if (_csp < kMaxCallStack) {
          _callStack[_csp++] = pc;
          int32_t target = static_cast<int32_t>(pc) + off;
          if (target >= 0 && target <= len) {
            pc = static_cast<uint16_t>(target);
          }
        }
        break;
      }
      case op::RET: {
        if (_csp > 0) {
          pc = _callStack[--_csp];
        } else {
          return; // return from top-level = halt
        }
        break;
      }
      case op::HALT: {
        return;
      }

      // ── Variables ─────────────────────────────────────────────────────────
      case op::LOAD: {
        if (pc >= len) return;
        uint8_t id = code[pc++] & 0x0F;
        push(_vars[id]);
        break;
      }
      case op::STORE: {
        if (pc >= len) return;
        uint8_t id = code[pc++] & 0x0F;
        _vars[id] = pop();
        break;
      }
      case op::INC: {
        if (pc >= len) return;
        uint8_t id = code[pc++] & 0x0F;
        _vars[id]++;
        break;
      }
      case op::DEC: {
        if (pc >= len) return;
        uint8_t id = code[pc++] & 0x0F;
        if (_vars[id] > 0) _vars[id]--;
        break;
      }

      // ── Grid ──────────────────────────────────────────────────────────────
      case op::GINIT: {
        if (pc + 1 >= len) return;
        uint8_t w = code[pc++];
        uint8_t h = code[pc++];
        _gridW = (w > 0 && w <= kMaxGridW) ? w : 8;
        _gridH = (h > 0 && h <= kMaxGridH) ? h : 6;
        memset(_grid, 0, sizeof(_grid));
        _curX = 0;
        _curY = 0;
        break;
      }
      case op::GSET: {
        int16_t y = pop();
        int16_t x = pop();
        int16_t val = pop();
        if (x >= 0 && x < _gridW && y >= 0 && y < _gridH) {
          _grid[y][x] = static_cast<uint8_t>(val);
        }
        break;
      }
      case op::GGET: {
        int16_t y = pop();
        int16_t x = pop();
        if (x >= 0 && x < _gridW && y >= 0 && y < _gridH) {
          push(_grid[y][x]);
        } else {
          push(0);
        }
        break;
      }
      case op::GFILL: {
        if (pc >= len) return;
        uint8_t val = code[pc++];
        for (int y = 0; y < _gridH; ++y) {
          for (int x = 0; x < _gridW; ++x) {
            _grid[y][x] = val;
          }
        }
        break;
      }
      case op::GCOUNT: {
        if (pc >= len) return;
        uint8_t val = code[pc++];
        int count = 0;
        for (int y = 0; y < _gridH; ++y) {
          for (int x = 0; x < _gridW; ++x) {
            if (_grid[y][x] == val) ++count;
          }
        }
        push(static_cast<int16_t>(count));
        break;
      }
      case op::GADJ: {
        int16_t y = pop();
        int16_t x = pop();
        push(static_cast<int16_t>(countAdj8(x, y)));
        break;
      }
      case op::GADJ4: {
        int16_t y = pop();
        int16_t x = pop();
        push(static_cast<int16_t>(countAdj4(x, y)));
        break;
      }
      case op::GFLOOD: {
        int16_t newVal = pop();
        int16_t oldVal = pop();
        int16_t y = pop();
        int16_t x = pop();
        floodFill(x, y, static_cast<uint8_t>(oldVal), static_cast<uint8_t>(newVal));
        break;
      }
      case op::GSWAP: {
        int16_t y2 = pop();
        int16_t x2 = pop();
        int16_t y1 = pop();
        int16_t x1 = pop();
        if (x1 >= 0 && x1 < _gridW && y1 >= 0 && y1 < _gridH &&
            x2 >= 0 && x2 < _gridW && y2 >= 0 && y2 < _gridH) {
          uint8_t temp = _grid[y1][x1];
          _grid[y1][x1] = _grid[y2][x2];
          _grid[y2][x2] = temp;
        }
        break;
      }
      case op::GROW: {
        int16_t y = pop();
        if (y >= 0 && y < _gridH) {
          for (int x = 0; x < _gridW; ++x) {
            push(_grid[y][x]);
          }
        }
        break;
      }

      // ── Cursor ────────────────────────────────────────────────────────────
      case op::CURX: {
        push(_curX);
        break;
      }
      case op::CURY: {
        push(_curY);
        break;
      }
      case op::CMOV: {
        int16_t y = pop();
        int16_t x = pop();
        if (x < 0) x = 0;
        if (x >= _gridW) x = _gridW - 1;
        if (y < 0) y = 0;
        if (y >= _gridH) y = _gridH - 1;
        _curX = static_cast<uint8_t>(x);
        _curY = static_cast<uint8_t>(y);
        break;
      }
      case op::CREL: {
        int16_t dy = pop();
        int16_t dx = pop();
        int nx = _curX + dx;
        int ny = _curY + dy;
        if (nx < 0) nx = 0;
        if (nx >= _gridW) nx = _gridW - 1;
        if (ny < 0) ny = 0;
        if (ny >= _gridH) ny = _gridH - 1;
        _curX = static_cast<uint8_t>(nx);
        _curY = static_cast<uint8_t>(ny);
        break;
      }

      // ── Random ────────────────────────────────────────────────────────────
      case op::RAND: {
        int16_t maxVal = pop();
        if (maxVal > 0) {
          push(static_cast<int16_t>(rngBelow(static_cast<uint32_t>(maxVal))));
        } else {
          push(0);
        }
        break;
      }

      // ── Rendering ─────────────────────────────────────────────────────────
      case op::CMAP: {
        if (pc + 1 >= len) return;
        uint8_t id = code[pc++] & 0x0F;
        char ch = static_cast<char>(code[pc++]);
        _cellChars[id] = ch;
        break;
      }
      case op::CSTYLE: {
        if (pc + 1 >= len) return;
        uint8_t id = code[pc++] & 0x0F;
        uint8_t st = code[pc++];
        _cellStyles[id] = st;
        break;
      }
      case op::TEXT: {
        if (pc >= len) return;
        _textId = static_cast<int8_t>(code[pc++]);
        break;
      }
      case op::TITLE: {
        if (pc >= len) return;
        _titleId = static_cast<int8_t>(code[pc++]);
        break;
      }
      case op::BAR: {
        if (pc >= len) return;
        _barVarId = static_cast<int8_t>(code[pc++] & 0x0F);
        break;
      }
      case op::SCORE: {
        _score = pop();
        break;
      }
      case op::FLASH: {
        _flash = true;
        break;
      }

      // ── Result ────────────────────────────────────────────────────────────
      case op::WIN: {
        _score = pop();
        _state = VMState::Won;
        return;
      }
      case op::LOSE: {
        _state = VMState::Lost;
        return;
      }
      case op::YIELD: {
        return;
      }

      default: {
        // Unknown opcode -> halt
        return;
      }
    }
  }
}

}  // namespace trail
