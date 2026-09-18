#pragma once

// Lume Trail — Micro-VM for minigame execution.
//
// A sandboxed, stack-based bytecode interpreter running in ~420 bytes of static
// RAM with zero heap allocations. Every minigame is data (bytecode + strings +
// cell styling) inside the story file, interpreted here.
//
// See docs/lume/15-trail-game-design.md §7 for the specification.

#include <cstdint>
#include <cstring>

namespace trail {

#ifdef DEC
#undef DEC
#endif
#ifdef HEX
#undef HEX
#endif
#ifdef BIN
#undef BIN
#endif

// Opcodes
namespace op {
  // Stack
  constexpr uint8_t PUSH   = 0x01; // [imm16 LE]
  constexpr uint8_t POP    = 0x02;
  constexpr uint8_t DUP    = 0x03;
  constexpr uint8_t SWAP   = 0x04;
  constexpr uint8_t OVER   = 0x05;

  // Arithmetic
  constexpr uint8_t ADD    = 0x10;
  constexpr uint8_t SUB    = 0x11;
  constexpr uint8_t MUL    = 0x12;
  constexpr uint8_t DIV    = 0x13;
  constexpr uint8_t MOD    = 0x14;
  constexpr uint8_t NEG    = 0x15;
  constexpr uint8_t ABS    = 0x16;
  constexpr uint8_t MIN    = 0x17;
  constexpr uint8_t MAX    = 0x18;
  constexpr uint8_t CLAMP  = 0x19;

  // Comparison
  constexpr uint8_t EQ     = 0x20;
  constexpr uint8_t NE     = 0x21;
  constexpr uint8_t LT     = 0x22;
  constexpr uint8_t GT     = 0x23;
  constexpr uint8_t LE     = 0x24;
  constexpr uint8_t GE     = 0x25;

  // Logic
  constexpr uint8_t AND    = 0x28;
  constexpr uint8_t OR     = 0x29;
  constexpr uint8_t NOT    = 0x2A;

  // Flow control
  constexpr uint8_t JMP    = 0x30; // [off16 LE signed]
  constexpr uint8_t JZ     = 0x31; // [off16 LE signed]
  constexpr uint8_t JNZ    = 0x32; // [off16 LE signed]
  constexpr uint8_t CALL   = 0x33; // [off16 LE signed]
  constexpr uint8_t RET    = 0x34;
  constexpr uint8_t HALT   = 0x35;

  // Variables
  constexpr uint8_t LOAD   = 0x40; // [id8]
  constexpr uint8_t STORE  = 0x41; // [id8]
  constexpr uint8_t INC    = 0x42; // [id8]
  constexpr uint8_t DEC    = 0x43; // [id8]

  // Grid
  constexpr uint8_t GINIT  = 0x50; // [w8, h8]
  constexpr uint8_t GSET   = 0x51; // pop val, y, x
  constexpr uint8_t GGET   = 0x52; // pop y, x -> push val
  constexpr uint8_t GFILL  = 0x53; // [val8]
  constexpr uint8_t GCOUNT = 0x54; // [val8] -> push count
  constexpr uint8_t GADJ   = 0x55; // pop y, x -> push count 8-adj != 0
  constexpr uint8_t GADJ4  = 0x56; // pop y, x -> push count 4-adj != 0
  constexpr uint8_t GFLOOD = 0x57; // pop new, old, y, x -> flood fill
  constexpr uint8_t GSWAP  = 0x58; // pop y2, x2, y1, x1 -> swap
  constexpr uint8_t GROW   = 0x59; // pop y -> push all cells in row (w values)

  // Cursor
  constexpr uint8_t CURX   = 0x60; // push curX
  constexpr uint8_t CURY   = 0x61; // push curY
  constexpr uint8_t CMOV   = 0x62; // pop y, x -> move cursor (clamped)
  constexpr uint8_t CREL   = 0x63; // pop dy, dx -> move relative (clamped)

  // Random
  constexpr uint8_t RAND   = 0x68; // pop max -> push [0, max)

  // Rendering
  constexpr uint8_t CMAP   = 0x70; // [id8, ch8]
  constexpr uint8_t CSTYLE = 0x71; // [id8, s8] (0=white, 1=black, 2=inv, 3=border)
  constexpr uint8_t TEXT   = 0x72; // [id8] set text string id
  constexpr uint8_t TITLE  = 0x73; // [id8] set title string id
  constexpr uint8_t BAR    = 0x74; // [id8] show bar for vars[id], max=vars[id+1]
  constexpr uint8_t SCORE  = 0x75; // pop score -> show score
  constexpr uint8_t FLASH  = 0x76; // request partial flash

  // Result
  constexpr uint8_t WIN    = 0x80; // pop score -> state=Won
  constexpr uint8_t LOSE   = 0x81; // state=Lost
  constexpr uint8_t YIELD  = 0x82; // yield (for future async)
}  // namespace op

enum class VMState : uint8_t {
  Fresh   = 0,
  Playing = 1,
  Won     = 2,
  Lost    = 3,
};

enum class Key : uint8_t {
  Up      = 0,
  Down    = 1,
  Left    = 2,
  Right   = 3,
  Confirm = 4,
  Back    = 5,
};

class TrailVM {
 public:
  static constexpr int kMaxGridW     = 16;
  static constexpr int kMaxGridH     = 16;
  static constexpr int kMaxGridCells = kMaxGridW * kMaxGridH; // 256
  static constexpr int kMaxVars      = 16;
  static constexpr int kMaxStack     = 32;
  static constexpr int kMaxCallStack = 8;
  static constexpr int kMaxCellTypes = 16;
  static constexpr int kMaxStrings   = 16;
  static constexpr int kMaxInstructions = 10000; // safety ceiling per cycle

  TrailVM();

  // Load a minigioco description. Bytecode pointers must remain valid while
  // the VM runs (they point into the loaded story file / flash).
  void load(const uint8_t* setupCode, uint16_t setupLen,
            const uint8_t* const keyCodes[6], const uint16_t keyLens[6],
            const char* const* stringTable, uint8_t stringCount,
            uint32_t rngSeed = 0x12345678u);

  // Initialize and run setup bytecode
  void start();

  // Process a key press (Key enum: Up/Down/Left/Right/Confirm/Back)
  void handleKey(Key key);

  // State inspection
  VMState state() const { return _state; }
  int16_t score() const { return _score; }
  bool isOver() const { return _state == VMState::Won || _state == VMState::Lost; }

  // Grid inspection
  uint8_t gridW() const { return _gridW; }
  uint8_t gridH() const { return _gridH; }
  uint8_t curX() const { return _curX; }
  uint8_t curY() const { return _curY; }
  uint8_t cell(int x, int y) const;
  char cellChar(uint8_t type) const { return _cellChars[type & 0x0F]; }
  uint8_t cellStyle(uint8_t type) const { return _cellStyles[type & 0x0F]; }

  // Variables & UI state
  int16_t var(int id) const { return _vars[id & 0x0F]; }
  int8_t textId() const { return _textId; }
  int8_t titleId() const { return _titleId; }
  int8_t barVarId() const { return _barVarId; }
  bool showBar() const { return _barVarId >= 0; }
  const char* string(int id) const;
  bool flashRequested() const { return _flash; }
  void clearFlash() { _flash = false; }

  // Direct stack/var manipulation for test harnesses
  int sp() const { return _sp; }
  int16_t stackTop() const { return _sp > 0 ? _stack[_sp - 1] : 0; }
  int16_t pop();
  void push(int16_t val);
  void setVar(int id, int16_t val) { _vars[id & 0x0F] = val; }
  void setRngSeed(uint32_t s) { _rng = s ? s : 0x9E3779B9u; }

 private:
  // Execution engine
  void execute(const uint8_t* code, uint16_t len);

  int16_t peek() const;

  // Grid helpers
  void floodFill(int x, int y, uint8_t oldVal, uint8_t newVal);
  int countAdj8(int x, int y) const;
  int countAdj4(int x, int y) const;

  // RNG (simple xorshift32)
  uint32_t nextRng();
  uint32_t rngBelow(uint32_t bound);

  // State
  uint8_t _grid[kMaxGridH][kMaxGridW] = {};
  uint8_t _gridW = 8;
  uint8_t _gridH = 6;
  uint8_t _curX  = 0;
  uint8_t _curY  = 0;

  int16_t _vars[kMaxVars] = {};
  int16_t _stack[kMaxStack] = {};
  uint8_t _sp = 0;

  uint16_t _callStack[kMaxCallStack] = {};
  uint8_t  _csp = 0;

  char    _cellChars[kMaxCellTypes] = {};
  uint8_t _cellStyles[kMaxCellTypes] = {};

  int8_t  _textId = -1;
  int8_t  _titleId = -1;
  int8_t  _barVarId = -1;
  bool    _flash = false;

  VMState _state = VMState::Fresh;
  int16_t _score = 0;

  // Bytecode pointers
  const uint8_t* _setupCode = nullptr;
  uint16_t       _setupLen = 0;
  const uint8_t* _keyCodes[6] = {};
  uint16_t       _keyLens[6] = {};

  const char* const* _strings = nullptr;
  uint8_t            _stringCount = 0;

  uint32_t _rng = 0x12345678u;
};

}  // namespace trail
