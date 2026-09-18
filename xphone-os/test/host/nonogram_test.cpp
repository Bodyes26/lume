// Host test for games::Nonogram — pack decode, clue derivation, cell state
// machine, row/col done indicators, solved detection, snapshot/restore.

#include <cassert>
#include <initializer_list>
#include <cstdio>
#include <cstring>

#include "games/Nonogram.cpp"

using games::Nonogram;
using games::NonoCell;
using games::Tier;

static int gChecks = 0;
#define CHECK(cond) \
  do {              \
    ++gChecks;      \
    assert(cond);   \
  } while (0)

int main() {
  // Test 1: Pack counts are non-zero
  CHECK(Nonogram::packCount(Tier::Easy) == 32);
  CHECK(Nonogram::packCount(Tier::Medium) == 32);
  CHECK(Nonogram::packCount(Tier::Hard) == 16);

  // Test 2: Verify every pack entry properties
  for (Tier t : {Tier::Easy, Tier::Medium, Tier::Hard}) {
    const uint32_t count = Nonogram::packCount(t);
    for (uint32_t i = 0; i < count; i++) {
      Nonogram n;
      n.load(t, i);
      CHECK(n.tier() == t);
      CHECK(n.index() == i);
      const int side = n.side();
      CHECK(side == 10 || side == 12 || side == 15);
      CHECK(n.targetCount() > 0);
      CHECK(!n.solved());

      for (int r = 0; r < side; r++) {
        uint8_t clues[Nonogram::kMaxClues];
        const int cnt = n.rowClues(r, clues);
        CHECK(cnt >= 0 && cnt <= Nonogram::kMaxClues);
      }
      for (int c = 0; c < side; c++) {
        uint8_t clues[Nonogram::kMaxClues];
        const int cnt = n.colClues(c, clues);
        CHECK(cnt >= 0 && cnt <= Nonogram::kMaxClues);
      }
    }
  }

  // Test 3: State machine cycleFill / cycleMark
  {
    Nonogram n;
    n.load(Tier::Easy, 0);
    CHECK(n.at(0, 0) == NonoCell::Empty);

    n.cycleFill(0, 0);
    CHECK(n.at(0, 0) == NonoCell::Fill);
    n.cycleFill(0, 0);
    CHECK(n.at(0, 0) == NonoCell::Empty);

    n.cycleMark(0, 0);
    CHECK(n.at(0, 0) == NonoCell::Mark);
    n.cycleMark(0, 0);
    CHECK(n.at(0, 0) == NonoCell::Empty);

    n.cycleFill(0, 0);
    CHECK(n.at(0, 0) == NonoCell::Fill);
    n.cycleMark(0, 0);
    CHECK(n.at(0, 0) == NonoCell::Mark);
  }

  // Test 4: Solved detection and clue done tracking
  {
    Nonogram n;
    n.load(Tier::Easy, 0);
    const int side = n.side();

    for (int r = 0; r < side; r++) {
      for (int c = 0; c < side; c++) {
        if (n.target(c, r)) {
          n.setCell(c, r, NonoCell::Fill);
        } else {
          n.setCell(c, r, NonoCell::Mark);
        }
      }
    }

    CHECK(n.solved());
    for (int r = 0; r < side; r++) CHECK(n.rowDone(r));
    for (int c = 0; c < side; c++) CHECK(n.colDone(c));
  }

  // Test 5: Snapshot and restore
  {
    Nonogram n1;
    n1.load(Tier::Medium, 10);
    n1.setCell(1, 1, NonoCell::Fill);
    n1.setCell(2, 2, NonoCell::Mark);

    uint8_t blob[Nonogram::kBlobBytes];
    n1.snapshot(blob);

    Nonogram n2;
    n2.load(Tier::Medium, 10);
    CHECK(n2.restore(blob));
    CHECK(n2.at(1, 1) == NonoCell::Fill);
    CHECK(n2.at(2, 2) == NonoCell::Mark);

    // Foreign index refusal
    Nonogram n3;
    n3.load(Tier::Medium, 11);
    CHECK(!n3.restore(blob));
  }

  printf("nonogram: all assertions passed (%d checks)\n", gChecks);
  return 0;
}
