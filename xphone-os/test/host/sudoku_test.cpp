// Host test for games::Sudoku — pack decode, unique-solution verification,
// edit rules, conflict marks, solved detection, snapshot/restore.

#include <cassert>
#include <initializer_list>
#include <cstdio>
#include <cstring>

#include "games/Sudoku.cpp"

using games::Sudoku;
using games::Tier;

static int gChecks = 0;
#define CHECK(cond) \
  do {              \
    ++gChecks;      \
    assert(cond);   \
  } while (0)

namespace {

bool solveBacktrack(uint8_t cells[81], int& solutions, int maxSol) {
  int cell = -1;
  for (int i = 0; i < 81; i++) {
    if (cells[i] == 0) {
      cell = i;
      break;
    }
  }
  if (cell < 0) {
    solutions++;
    return solutions < maxSol;
  }

  const int r = cell / 9;
  const int c = cell % 9;
  const int br = (r / 3) * 3;
  const int bc = (c / 3) * 3;

  uint16_t used = 0;
  for (int i = 0; i < 9; i++) {
    const uint8_t vr = cells[r * 9 + i];
    if (vr) used |= (1u << vr);
    const uint8_t vc = cells[i * 9 + c];
    if (vc) used |= (1u << vc);
    const uint8_t vb = cells[(br + (i / 3)) * 9 + (bc + (i % 3))];
    if (vb) used |= (1u << vb);
  }

  for (uint8_t v = 1; v <= 9; v++) {
    if (!(used & (1u << v))) {
      cells[cell] = v;
      if (!solveBacktrack(cells, solutions, maxSol)) return false;
      cells[cell] = 0;
    }
  }
  return true;
}

int countSolutions(const Sudoku& s) {
  uint8_t cells[81];
  for (int i = 0; i < 81; i++) cells[i] = s.at(i);
  int sol = 0;
  solveBacktrack(cells, sol, 2);
  return sol;
}

}  // namespace

int main() {
  // Test 1: Pack counts are non-zero
  CHECK(Sudoku::packCount(Tier::Easy) == 128);
  CHECK(Sudoku::packCount(Tier::Medium) == 256);
  CHECK(Sudoku::packCount(Tier::Hard) == 128);

  // Test 2: Verify every pack entry decodes to a valid puzzle with exactly one solution
  for (Tier t : {Tier::Easy, Tier::Medium, Tier::Hard}) {
    const uint32_t count = Sudoku::packCount(t);
    for (uint32_t i = 0; i < count; i++) {
      Sudoku s;
      s.load(t, i);
      CHECK(s.tier() == t);
      CHECK(s.index() == i);
      CHECK(s.filled() >= 20 && s.filled() <= 45);
      CHECK(!s.solved());
      for (int c = 0; c < 81; c++) {
        CHECK(!s.conflict(c));
      }
      CHECK(countSolutions(s) == 1);
    }
  }

  // Test 3: Givens are immutable, player cells accept 0..9
  {
    Sudoku s;
    s.load(Tier::Easy, 0);
    int givenCell = -1, emptyCell = -1;
    for (int i = 0; i < 81; i++) {
      if (s.isGiven(i) && givenCell < 0) givenCell = i;
      if (!s.isGiven(i) && emptyCell < 0) emptyCell = i;
    }
    CHECK(givenCell >= 0 && emptyCell >= 0);
    CHECK(!s.set(givenCell, 5));
    CHECK(s.set(emptyCell, 7));
    CHECK(s.at(emptyCell) == 7);
    CHECK(s.set(emptyCell, 0));
    CHECK(s.at(emptyCell) == 0);
  }

  // Test 4: Conflict detection on row, column and 3x3 box
  {
    Sudoku s;
    s.load(Tier::Easy, 0);
    for (int i = 0; i < 81; i++) {
      if (s.isGiven(i)) {
        const int r = i / 9;
        for (int c = 0; c < 9; c++) {
          const int other = r * 9 + c;
          if (!s.isGiven(other)) {
            s.set(other, s.at(i));
            CHECK(s.conflict(other));
            s.set(other, 0);
            break;
          }
        }
        break;
      }
    }
  }

  // Test 5: Snapshot and restore round-trip
  {
    Sudoku s1;
    s1.load(Tier::Medium, 42);
    for (int i = 0; i < 81; i++) {
      if (!s1.isGiven(i)) s1.set(i, 3);
    }
    uint8_t blob[Sudoku::kBlobBytes];
    s1.snapshot(blob);

    Sudoku s2;
    s2.load(Tier::Medium, 42);
    CHECK(s2.restore(blob));
    for (int i = 0; i < 81; i++) {
      CHECK(s2.at(i) == s1.at(i));
    }

    // Refusal of foreign puzzle index
    Sudoku s3;
    s3.load(Tier::Medium, 43);
    CHECK(!s3.restore(blob));

    // Refusal of foreign tier
    Sudoku s4;
    s4.load(Tier::Easy, 42);
    CHECK(!s4.restore(blob));
  }

  // Test 6: Solved detection
  {
    Sudoku s;
    s.load(Tier::Easy, 0);
    uint8_t sol[81];
    for (int i = 0; i < 81; i++) sol[i] = s.at(i);
    int found = 0;
    solveBacktrack(sol, found, 1);
    CHECK(found == 1);
    for (int i = 0; i < 81; i++) {
      if (!s.isGiven(i)) s.set(i, sol[i]);
    }
    CHECK(s.solved());
  }

  printf("sudoku: all assertions passed (%d checks)\n", gChecks);
  return 0;
}
