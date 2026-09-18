// Host test for trail::TrailVM micro-VM bytecode interpreter.
//
// Asserts all 40+ opcodes, bounds checking, stack safety, call stack,
// grid operations (flood fill, adjacencies), infinite loop protection,
// and a full interactive minigame simulation.

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "games/trail/TrailVM.cpp"

using namespace trail;
static int gChecks = 0;
#define CHECK(cond)                                            \
  do {                                                         \
    ++gChecks;                                                 \
    const bool _ok = (cond);                                   \
    if (!_ok) {                                                \
      printf("FAILED check at %s:%d\n", __FILE__, __LINE__);   \
      assert(false);                                           \
    }                                                          \
  } while (0)

namespace {

void testStackAndArithmetic() {
  TrailVM vm;

  // PUSH 10, PUSH 20, ADD, PUSH 5, SUB, PUSH 2, MUL, PUSH 10, DIV, HALT
  // (10 + 20 - 5) * 2 / 10 = 25 * 2 / 10 = 5
  uint8_t code[] = {
    op::PUSH, 10, 0,
    op::PUSH, 20, 0,
    op::ADD,
    op::PUSH, 5, 0,
    op::SUB,
    op::PUSH, 2, 0,
    op::MUL,
    op::PUSH, 10, 0,
    op::DIV,
    op::HALT
  };

  vm.load(code, sizeof(code), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.sp() == 1);
  CHECK(vm.stackTop() == 5);

  // Stack ops: PUSH 42, DUP, PUSH 100, SWAP, OVER, HALT -> stack: [42, 100, 42, 100]
  uint8_t code2[] = {
    op::PUSH, 42, 0,
    op::DUP,
    op::PUSH, 100, 0,
    op::SWAP,
    op::OVER,
    op::HALT
  };
  vm.load(code2, sizeof(code2), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.sp() == 4);
  CHECK(vm.pop() == 100);
  CHECK(vm.pop() == 42);
  CHECK(vm.pop() == 100);
  CHECK(vm.pop() == 42);

  // Div by zero and Mod by zero safety
  uint8_t codeDivZero[] = {
    op::PUSH, 100, 0,
    op::PUSH, 0, 0,
    op::DIV,
    op::PUSH, 100, 0,
    op::PUSH, 0, 0,
    op::MOD,
    op::HALT
  };
  vm.load(codeDivZero, sizeof(codeDivZero), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.pop() == 0);
  CHECK(vm.pop() == 0);

  // NEG, ABS, MIN, MAX, CLAMP
  uint8_t codeMath[] = {
    op::PUSH, 15, 0,
    op::NEG,           // -15
    op::ABS,           // 15
    op::PUSH, 20, 0,
    op::MIN,           // min(15, 20) = 15
    op::PUSH, 8, 0,
    op::MAX,           // max(15, 8) = 15
    op::PUSH, 50, 0,   // val = 50
    op::PUSH, 10, 0,   // lo = 10
    op::PUSH, 30, 0,   // hi = 30
    op::CLAMP,         // clamp(50, 10, 30) = 30
    op::HALT
  };
  vm.load(codeMath, sizeof(codeMath), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.pop() == 30);
  CHECK(vm.pop() == 15);
}

void testComparisonAndLogic() {
  TrailVM vm;

  // EQ, NE, LT, GT, LE, GE
  uint8_t code[] = {
    op::PUSH, 10, 0, op::PUSH, 10, 0, op::EQ, // 1
    op::PUSH, 10, 0, op::PUSH, 20, 0, op::NE, // 1
    op::PUSH, 5,  0, op::PUSH, 10, 0, op::LT, // 1
    op::PUSH, 20, 0, op::PUSH, 10, 0, op::GT, // 1
    op::PUSH, 10, 0, op::PUSH, 10, 0, op::LE, // 1
    op::PUSH, 10, 0, op::PUSH, 10, 0, op::GE, // 1
    op::HALT
  };
  vm.load(code, sizeof(code), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.sp() == 6);
  for (int i = 0; i < 6; ++i) {
    CHECK(vm.pop() == 1);
  }

  // Logic: AND, OR, NOT
  uint8_t codeLogic[] = {
    op::PUSH, 1, 0, op::PUSH, 0, 0, op::AND, // 0
    op::PUSH, 1, 0, op::PUSH, 0, 0, op::OR,  // 1
    op::PUSH, 0, 0, op::NOT,                 // 1
    op::PUSH, 1, 0, op::NOT,                 // 0
    op::HALT
  };
  vm.load(codeLogic, sizeof(codeLogic), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.pop() == 0);
  CHECK(vm.pop() == 1);
  CHECK(vm.pop() == 1);
  CHECK(vm.pop() == 0);
}

void testVariablesAndLoops() {
  TrailVM vm;

  // Loop: sum 1 to 10 into var 0
  // vars[0] = 0 (sum), vars[1] = 10 (counter)
  // loop:
  //   vars[0] += vars[1]
  //   vars[1]--
  //   if (vars[1] > 0) goto loop
  // HALT
  uint8_t code[] = {
    op::PUSH, 0, 0,
    op::STORE, 0,        // vars[0] = 0
    op::PUSH, 10, 0,
    op::STORE, 1,        // vars[1] = 10

    // @loop (offset 8)
    op::LOAD, 0,
    op::LOAD, 1,
    op::ADD,
    op::STORE, 0,        // vars[0] += vars[1]

    op::DEC, 1,          // vars[1]--
    op::LOAD, 1,
    op::PUSH, 0, 0,
    op::GT,              // vars[1] > 0 ?
    op::JNZ, static_cast<uint8_t>(-18 & 0xFF), static_cast<uint8_t>((-18 >> 8) & 0xFF),

    op::LOAD, 0,
    op::HALT
  };

  vm.load(code, sizeof(code), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.var(0) == 55); // 10 * 11 / 2 = 55
  CHECK(vm.pop() == 55);
}

void testCallAndRet() {
  TrailVM vm;

  // main:
  //   PUSH 5
  //   CALL @double_it
  //   PUSH 10
  //   CALL @double_it
  //   ADD
  //   HALT
  // double_it:
  //   PUSH 2
  //   MUL
  //   RET
  // (5*2) + (10*2) = 30
  uint8_t code[] = {
    // 0: PUSH 5
    op::PUSH, 5, 0,
    // 3: CALL +13 -> offset 3+3+13 = 19
    op::CALL, 13, 0,
    // 6: PUSH 10
    op::PUSH, 10, 0,
    // 9: CALL +7 -> offset 9+3+7 = 19
    op::CALL, 7, 0,
    // 12: ADD
    op::ADD,
    // 13: HALT
    op::HALT,

    // padding / unused
    0, 0, 0, 0, 0,

    // 19: @double_it
    op::PUSH, 2, 0,
    op::MUL,
    op::RET
  };

  vm.load(code, sizeof(code), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.pop() == 30);
}

void testGridOperations() {
  TrailVM vm;

  // GINIT 6, 6
  // GSET (1, 1) = 2, GSET (1, 2) = 2, GSET (2, 1) = 2
  // GCOUNT 2 -> 3
  // GADJ (0, 0) -> 1 (touches (1,1))
  // GADJ (1, 1) -> 2 (touches (1,2) and (2,1))
  uint8_t code[] = {
    op::GINIT, 6, 6,
    op::PUSH, 2, 0, op::PUSH, 1, 0, op::PUSH, 1, 0, op::GSET, // (1,1) = 2
    op::PUSH, 2, 0, op::PUSH, 2, 0, op::PUSH, 1, 0, op::GSET, // (1,2) = 2
    op::PUSH, 2, 0, op::PUSH, 1, 0, op::PUSH, 2, 0, op::GSET, // (2,1) = 2
    op::GCOUNT, 2,                                             // count 2 -> 3
    op::PUSH, 0, 0, op::PUSH, 0, 0, op::GADJ,                  // adj(0,0) -> 1
    op::PUSH, 1, 0, op::PUSH, 1, 0, op::GADJ,                  // adj(1,1) -> 2
    op::HALT
  };

  vm.load(code, sizeof(code), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.pop() == 2); // adj(1,1)
  CHECK(vm.pop() == 1); // adj(0,0)
  CHECK(vm.pop() == 3); // count 2

  // Flood fill test: GFILL 1, GSET (0,0)=0, GFLOOD(0,0, 0->5)
  uint8_t codeFlood[] = {
    op::GINIT, 4, 4,
    op::GFILL, 1,
    op::PUSH, 0, 0, op::PUSH, 0, 0, op::PUSH, 0, 0, op::GSET, // (0,0) = 0
    op::PUSH, 0, 0, op::PUSH, 1, 0, op::PUSH, 0, 0, op::GSET, // (1,0) = 0
    op::PUSH, 0, 0, op::PUSH, 0, 0, op::PUSH, 1, 0, op::GSET, // (0,1) = 0
    op::PUSH, 0, 0, op::PUSH, 0, 0, op::PUSH, 0, 0, op::PUSH, 5, 0, op::GFLOOD, // flood x=0, y=0, 0->5
    op::GCOUNT, 5,
    op::HALT
  };
  vm.load(codeFlood, sizeof(codeFlood), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.pop() == 3); // 3 cells were flooded
  CHECK(vm.cell(0, 0) == 5);
  CHECK(vm.cell(1, 0) == 5);
  CHECK(vm.cell(0, 1) == 5);
  CHECK(vm.cell(1, 1) == 1); // untouched
}

void testCursorMovement() {
  TrailVM vm;

  // GINIT 8, 6
  // CMOV 3, 2 -> (3, 2)
  // CREL 1, -1 -> (4, 1)
  // CREL 10, 10 -> (7, 5) [clamped]
  // CREL -20, -20 -> (0, 0) [clamped]
  uint8_t code[] = {
    op::GINIT, 8, 6,
    op::PUSH, 3, 0, op::PUSH, 2, 0, op::CMOV,
    op::CURX, op::CURY,
    op::PUSH, 1, 0, op::PUSH, 0xFF, 0xFF, op::CREL,
    op::CURX, op::CURY,
    op::PUSH, 10, 0, op::PUSH, 10, 0, op::CREL,
    op::CURX, op::CURY,
    op::PUSH, 0xEC, 0xFF, op::PUSH, 0xEC, 0xFF, op::CREL,
    op::CURX, op::CURY,
    op::HALT
  };

  vm.load(code, sizeof(code), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.pop() == 0); CHECK(vm.pop() == 0); // (0,0)
  CHECK(vm.pop() == 5); CHECK(vm.pop() == 7); // (7,5)
  CHECK(vm.pop() == 1); CHECK(vm.pop() == 4); // (4,1)
  CHECK(vm.pop() == 2); CHECK(vm.pop() == 3); // (3,2)
}

void testFullMinigameSimulation() {
  // A complete simulation of the "Caccia" minigame:
  // Setup: 4x4 grid, 3 hidden targets (type 2) at (1,1), (2,2), (3,3).
  // vars[0] = 3 shots remaining, vars[1] = 0 hits
  // Up/Down/Left/Right move cursor
  // Confirm: shoot current cell
  //   if cell == 2 -> cell=3 (hit), hits++, "Colpita!"
  //   else -> cell=4 (miss), "A vuoto..."
  //   shots--
  //   if hits >= 2 -> WIN(hits)
  //   else if shots == 0 -> LOSE
  TrailVM vm;

  const char* strings[] = {"Colpita!", "A vuoto...", "Caccia al fiume"};

  uint8_t setupCode[] = {
    op::GINIT, 4, 4,
    op::PUSH, 2, 0, op::PUSH, 1, 0, op::PUSH, 1, 0, op::GSET, // target at (1,1)
    op::PUSH, 2, 0, op::PUSH, 2, 0, op::PUSH, 2, 0, op::GSET, // target at (2,2)
    op::PUSH, 2, 0, op::PUSH, 3, 0, op::PUSH, 3, 0, op::GSET, // target at (3,3)
    op::PUSH, 3, 0, op::STORE, 0, // shots = 3
    op::PUSH, 0, 0, op::STORE, 1, // hits = 0
    op::TITLE, 2,
    op::BAR, 0,
    op::HALT
  };

  // Up: dy = -1, dx = 0
  uint8_t upCode[] = { op::PUSH, 0, 0, op::PUSH, 0xFF, 0xFF, op::CREL, op::HALT };
  // Down: dy = 1, dx = 0
  uint8_t downCode[] = { op::PUSH, 0, 0, op::PUSH, 1, 0, op::CREL, op::HALT };
  // Left: dy = 0, dx = -1
  uint8_t leftCode[] = { op::PUSH, 0xFF, 0xFF, op::PUSH, 0, 0, op::CREL, op::HALT };
  // Right: dy = 1, dx = 1
  uint8_t rightCode[] = { op::PUSH, 1, 0, op::PUSH, 0, 0, op::CREL, op::HALT };

  // Confirm: shoot
  uint8_t confirmCode[] = {
    // 0
    op::CURX, op::CURY, op::GGET, // get cell under cursor (3 B)
    // 3
    op::DUP,                      // (1 B)
    // 4
    op::PUSH, 2, 0,               // (3 B)
    // 7
    op::EQ,                       // (1 B)
    // 8: JZ @miss (PC=11, target=25, offset = 14)
    op::JZ, 14, 0,                // (3 B)

    // @hit (11)
    op::POP,                      // (1 B)
    op::PUSH, 3, 0,               // (3 B) [12,13,14]
    op::CURX, op::CURY, op::GSET, // (3 B) [15,16,17]
    op::INC, 1,                   // (2 B) [18,19] hits++
    op::TEXT, 0,                  // (2 B) [20,21] "Colpita!"
    // 22: JMP @common (PC=25, target=34, offset = 9)
    op::JMP, 9, 0,                // (3 B)

    // @miss (25)
    op::POP,                      // (1 B)
    op::PUSH, 4, 0,               // (3 B) [26,27,28]
    op::CURX, op::CURY, op::GSET, // (3 B) [29,30,31]
    op::TEXT, 1,                  // (2 B) [32,33] "A vuoto..."

    // @common (34)
    op::DEC, 0,                   // (2 B) [34,35] shots--

    // Check win: hits >= 2? (36)
    op::LOAD, 1,                  // (2 B) [36,37]
    op::PUSH, 2, 0,               // (3 B) [38,39,40]
    op::GE,                       // (1 B) [41]
    // 42: JZ @check_lose (PC=45, target=48, offset = 3)
    op::JZ, 3, 0,                 // (3 B) [42,43,44]
    op::LOAD, 1,                  // (2 B) [45,46]
    op::WIN,                      // (1 B) [47]

    // @check_lose (48)
    op::LOAD, 0,                  // (2 B) [48,49]
    op::PUSH, 0, 0,               // (3 B) [50,51,52]
    op::LE,                       // (1 B) [53]
    // 54: JZ @done (PC=57, target=58, offset = 1)
    op::JZ, 1, 0,                 // (3 B) [54,55,56]
    op::LOSE,                     // (1 B) [57]

    // @done (58)
    op::HALT                      // (1 B) [58]
  };

  const uint8_t* keyCodes[6] = {upCode, downCode, leftCode, rightCode, confirmCode, nullptr};
  const uint16_t keyLens[6]  = {sizeof(upCode), sizeof(downCode), sizeof(leftCode), sizeof(rightCode), sizeof(confirmCode), 0};

  vm.load(setupCode, sizeof(setupCode), keyCodes, keyLens, strings, 3);
  vm.start();

  CHECK(vm.state() == VMState::Playing);
  CHECK(vm.var(0) == 3); // 3 shots
  CHECK(vm.var(1) == 0); // 0 hits
  CHECK(vm.curX() == 0 && vm.curY() == 0);

  // Move to (1,1): Right, Down
  vm.handleKey(Key::Right);
  vm.handleKey(Key::Down);
  CHECK(vm.curX() == 1 && vm.curY() == 1);

  // Shoot at (1,1) -> HIT!
  vm.handleKey(Key::Confirm);
  CHECK(vm.var(1) == 1); // 1 hit
  CHECK(vm.var(0) == 2); // 2 shots left
  CHECK(vm.cell(1, 1) == 3); // cell is hit mark
  CHECK(vm.state() == VMState::Playing);

  // Move to (0,0) and shoot -> MISS!
  vm.handleKey(Key::Left);
  vm.handleKey(Key::Up);
  vm.handleKey(Key::Confirm);
  CHECK(vm.var(1) == 1); // still 1 hit
  CHECK(vm.var(0) == 1); // 1 shot left
  CHECK(vm.cell(0, 0) == 4); // miss mark
  CHECK(vm.state() == VMState::Playing);

  // Move to (2,2): Down, Down, Right, Right -> shoot -> HIT! -> WIN!
  vm.handleKey(Key::Down);
  vm.handleKey(Key::Down);
  vm.handleKey(Key::Right);
  vm.handleKey(Key::Right);
  CHECK(vm.curX() == 2 && vm.curY() == 2);
  vm.handleKey(Key::Confirm);

  CHECK(vm.state() == VMState::Won);
  CHECK(vm.score() == 2);
}

void testSafetyLimits() {
  TrailVM vm;

  // Infinite loop protection: JMP -3
  uint8_t loopCode[] = {
    op::JMP, static_cast<uint8_t>(-3 & 0xFF), static_cast<uint8_t>((-3 >> 8) & 0xFF)
  };
  vm.load(loopCode, sizeof(loopCode), nullptr, nullptr, nullptr, 0);
  vm.start(); // Must terminate safely without hanging
  CHECK(true);

  // Stack underflow protection: POP on empty stack
  uint8_t popEmpty[] = {
    op::POP, op::POP, op::POP, op::HALT
  };
  vm.load(popEmpty, sizeof(popEmpty), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.sp() == 0);

  // Stack overflow protection: push 50 values (stack limit is 32)
  std::vector<uint8_t> overflowCode;
  for (int i = 0; i < 50; ++i) {
    overflowCode.push_back(op::PUSH);
    overflowCode.push_back(static_cast<uint8_t>(i));
    overflowCode.push_back(0);
  }
  overflowCode.push_back(op::HALT);
  vm.load(overflowCode.data(), overflowCode.size(), nullptr, nullptr, nullptr, 0);
  vm.start();
  CHECK(vm.sp() == TrailVM::kMaxStack);
}

}  // namespace

int main() {
  testStackAndArithmetic();
  testComparisonAndLogic();
  testVariablesAndLoops();
  testCallAndRet();
  testGridOperations();
  testCursorMovement();
  testFullMinigameSimulation();
  testSafetyLimits();

  printf("trail_vm_test: %d checks passed\n", gChecks);
  return 0;
}
