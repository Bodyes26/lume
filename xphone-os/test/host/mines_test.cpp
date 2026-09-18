// Host test for games::Mines — the whole point of a pure-logic minefield is
// that the rules can be asserted without a panel: first-reveal safety, exact
// mine count, an iterative flood that stops at numbers, and a board that is
// rebuilt from (seed, first cell) instead of persisting its mines.

#include <cassert>
#include <cstdio>
#include <cstring>

#include "games/Mines.cpp"

using games::Mines;
using games::Tier;

static int gChecks = 0;
#define CHECK(cond)      \
  do {                   \
    ++gChecks;           \
    assert(cond);        \
  } while (0)

namespace {

constexpr Tier kTiers[] = {Tier::Easy, Tier::Medium, Tier::Hard};

int mineCount(const Mines& m) {
  int n = 0;
  for (int r = 0; r < m.rows(); ++r) {
    for (int c = 0; c < m.cols(); ++c) {
      if (m.mine(c, r)) ++n;
    }
  }
  return n;
}

// 8-connected reachability over revealed cells, so "the flood opened one region"
// is checked rather than assumed.
int reachableRevealed(const Mines& m, int startCol, int startRow) {
  bool seen[Mines::kMaxCells] = {};
  int stack[Mines::kMaxCells];
  int sp = 0;
  const int start = startRow * m.cols() + startCol;
  seen[start] = true;
  stack[sp++] = start;
  int n = 0;
  while (sp > 0) {
    const int cell = stack[--sp];
    ++n;
    const int col = cell % m.cols();
    const int row = cell / m.cols();
    for (int dr = -1; dr <= 1; ++dr) {
      for (int dc = -1; dc <= 1; ++dc) {
        const int nc = col + dc;
        const int nr = row + dr;
        if (nc < 0 || nr < 0 || nc >= m.cols() || nr >= m.rows()) continue;
        const int i = nr * m.cols() + nc;
        if (seen[i] || !m.revealed(nc, nr)) continue;
        seen[i] = true;
        stack[sp++] = i;
      }
    }
  }
  return n;
}

int revealedTotal(const Mines& m) {
  int n = 0;
  for (int r = 0; r < m.rows(); ++r) {
    for (int c = 0; c < m.cols(); ++c) {
      if (m.revealed(c, r)) ++n;
    }
  }
  return n;
}

}  // namespace

int main() {
  // Test 1: every shape fits the arrays the model reserves, and leaves room for
  // the 3x3 safe patch the first reveal needs.
  for (Tier t : kTiers) {
    const Mines::Shape s = Mines::shapeFor(t);
    CHECK(s.cols >= 1 && s.cols <= Mines::kMaxCols);
    CHECK(s.rows >= 1 && s.rows <= Mines::kMaxRows);
    CHECK(s.cols * s.rows <= Mines::kMaxCells);
    CHECK(s.mines > 0 && s.mines + 9 <= s.cols * s.rows);
  }
  {
    // Harder tiers must actually be harder (more mines per cell).
    const Mines::Shape e = Mines::shapeFor(Tier::Easy);
    const Mines::Shape m = Mines::shapeFor(Tier::Medium);
    const Mines::Shape h = Mines::shapeFor(Tier::Hard);
    CHECK(e.mines * (m.cols * m.rows) < m.mines * (e.cols * e.rows));
    CHECK(m.mines * (h.cols * h.rows) < h.mines * (m.cols * m.rows));
  }

  // Test 2: a fresh board hides nothing yet — mine()/adjacent() must not leak a
  // field that has not been laid.
  {
    Mines m;
    m.load(Tier::Medium, 12345u);
    CHECK(m.state() == Mines::State::Fresh);
    CHECK(m.cols() == Mines::shapeFor(Tier::Medium).cols);
    CHECK(m.rows() == Mines::shapeFor(Tier::Medium).rows);
    CHECK(m.mines() == Mines::shapeFor(Tier::Medium).mines);
    CHECK(mineCount(m) == 0);
    CHECK(m.adjacent(0, 0) == 0);
    CHECK(m.revealedCount() == 0);
    CHECK(m.minesLeft() == m.mines());
  }

  // Test 3: first-reveal safety and exact mine count, over many seeds and every
  // tier — the property that makes move one unloseable.
  for (Tier t : kTiers) {
    for (uint32_t seed = 1; seed <= 120; ++seed) {
      Mines m;
      m.load(t, seed * 2654435761u);
      const int fc = static_cast<int>(seed % static_cast<uint32_t>(m.cols()));
      const int fr = static_cast<int>((seed * 7u) % static_cast<uint32_t>(m.rows()));
      m.reveal(fc, fr);
      CHECK(m.state() != Mines::State::Lost);
      CHECK(mineCount(m) == m.mines());
      for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
          CHECK(!m.mine(fc + dc, fr + dr));
        }
      }
      CHECK(m.adjacent(fc, fr) == 0);
      CHECK(m.revealed(fc, fr));

      // The flood is one contiguous region, bordered by numbers, and never
      // opens a mine.
      CHECK(reachableRevealed(m, fc, fr) == revealedTotal(m));
      for (int r = 0; r < m.rows(); ++r) {
        for (int c = 0; c < m.cols(); ++c) {
          if (!m.revealed(c, r)) continue;
          assert(!m.mine(c, r));
          if (m.adjacent(c, r) != 0) continue;
          for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
              const int nc = c + dc;
              const int nr = r + dr;
              if (nc < 0 || nr < 0 || nc >= m.cols() || nr >= m.rows()) continue;
              assert(m.revealed(nc, nr));  // a zero always opens all its neighbours
            }
          }
        }
      }
      ++gChecks;
    }
  }

  // Test 4: same (seed, first cell) rebuilds an identical field. This is what
  // lets the snapshot store 48 bytes instead of the mine mask.
  {
    Mines a, b;
    a.load(Tier::Hard, 0xC0FFEEu);
    b.load(Tier::Hard, 0xC0FFEEu);
    a.reveal(3, 4);
    b.reveal(3, 4);
    for (int r = 0; r < a.rows(); ++r) {
      for (int c = 0; c < a.cols(); ++c) {
        assert(a.mine(c, r) == b.mine(c, r));
        assert(a.revealed(c, r) == b.revealed(c, r));
      }
    }
    ++gChecks;

    // A different first cell on the same seed is a different board (the safe
    // patch moves), which is why the first cell is part of the identity.
    Mines d;
    d.load(Tier::Hard, 0xC0FFEEu);
    d.reveal(9, 11);
    int diff = 0;
    for (int r = 0; r < a.rows(); ++r) {
      for (int c = 0; c < a.cols(); ++c) {
        if (a.mine(c, r) != d.mine(c, r)) ++diff;
      }
    }
    CHECK(diff > 0);
  }

  // Test 5: a zero floods, a numbered cell does not.
  {
    Mines m;
    m.load(Tier::Medium, 987654321u);
    m.reveal(0, 0);
    CHECK(m.revealedCount() > 1);  // the safe patch guarantees at least the 3x3

    // Find an unrevealed numbered cell and open it: exactly one more cell.
    int before = m.revealedCount();
    bool tested = false;
    for (int r = 0; r < m.rows() && !tested; ++r) {
      for (int c = 0; c < m.cols() && !tested; ++c) {
        if (m.revealed(c, r) || m.mine(c, r) || m.adjacent(c, r) == 0) continue;
        m.reveal(c, r);
        CHECK(m.revealed(c, r));
        CHECK(m.revealedCount() == before + 1);
        tested = true;
      }
    }
    CHECK(tested);

    // Re-revealing is a no-op.
    before = m.revealedCount();
    m.reveal(0, 0);
    CHECK(m.revealedCount() == before);
  }

  // Test 6: a flag protects its cell, from a direct reveal and from the flood.
  {
    Mines m;
    m.load(Tier::Easy, 55u);
    m.reveal(0, 0);
    const int floodSize = m.revealedCount();
    CHECK(floodSize > 2);

    // Same board again, this time with one cell of that flood flagged.
    int flagCol = -1, flagRow = -1;
    for (int r = 0; r < m.rows() && flagCol < 0; ++r) {
      for (int c = 0; c < m.cols(); ++c) {
        if (m.revealed(c, r) && !(c == 0 && r == 0)) {
          flagCol = c;
          flagRow = r;
          break;
        }
      }
    }
    CHECK(flagCol >= 0);

    Mines n;
    n.load(Tier::Easy, 55u);
    n.toggleFlag(flagCol, flagRow);
    CHECK(n.flagged(flagCol, flagRow));
    CHECK(n.flagsPlaced() == 1);
    CHECK(n.minesLeft() == n.mines() - 1);
    n.reveal(0, 0);
    CHECK(!n.revealed(flagCol, flagRow));
    CHECK(n.revealedCount() < floodSize);

    // A direct reveal of a flagged cell is a no-op, and un-flagging restores it.
    n.reveal(flagCol, flagRow);
    CHECK(!n.revealed(flagCol, flagRow));
    n.toggleFlag(flagCol, flagRow);
    CHECK(!n.flagged(flagCol, flagRow));
    n.reveal(flagCol, flagRow);
    CHECK(n.revealed(flagCol, flagRow));

    // A revealed cell cannot be flagged.
    n.toggleFlag(flagCol, flagRow);
    CHECK(!n.flagged(flagCol, flagRow));
  }

  // Test 7: revealing a mine loses the game and freezes the board.
  {
    Mines m;
    m.load(Tier::Medium, 424242u);
    m.reveal(1, 1);
    int mineCol = -1, mineRow = -1;
    for (int r = 0; r < m.rows() && mineCol < 0; ++r) {
      for (int c = 0; c < m.cols(); ++c) {
        if (m.mine(c, r)) {
          mineCol = c;
          mineRow = r;
          break;
        }
      }
    }
    CHECK(mineCol >= 0);
    const int before = m.revealedCount();
    m.reveal(mineCol, mineRow);
    CHECK(m.state() == Mines::State::Lost);
    CHECK(m.revealed(mineCol, mineRow));
    CHECK(m.revealedCount() == before + 1);

    // Frozen: no reveal, no flag.
    int freeCol = -1, freeRow = -1;
    for (int r = 0; r < m.rows() && freeCol < 0; ++r) {
      for (int c = 0; c < m.cols(); ++c) {
        if (!m.revealed(c, r) && !m.mine(c, r)) {
          freeCol = c;
          freeRow = r;
          break;
        }
      }
    }
    CHECK(freeCol >= 0);
    m.reveal(freeCol, freeRow);
    CHECK(!m.revealed(freeCol, freeRow));
    m.toggleFlag(freeCol, freeRow);
    CHECK(!m.flagged(freeCol, freeRow));
    CHECK(m.state() == Mines::State::Lost);
  }

  // Test 8: Won exactly when every non-mine cell is open — not one reveal
  // earlier, and flags are irrelevant to it.
  {
    Mines m;
    m.load(Tier::Easy, 31337u);
    m.reveal(0, 0);
    const int safeCells = m.cols() * m.rows() - m.mines();
    for (int r = 0; r < m.rows(); ++r) {
      for (int c = 0; c < m.cols(); ++c) {
        if (m.mine(c, r)) continue;
        m.reveal(c, r);
        assert((m.state() == Mines::State::Won) == (m.revealedCount() == safeCells));
      }
    }
    CHECK(m.state() == Mines::State::Won);
    CHECK(m.revealedCount() == safeCells);

    // A won board is frozen too.
    m.toggleFlag(0, 1);
    CHECK(!m.flagged(0, 1));
  }

  // Test 9: snapshot/restore reproduces the field, the masks and the state.
  {
    Mines m;
    m.load(Tier::Medium, 777u);
    m.reveal(4, 5);
    m.toggleFlag(0, 0);
    m.toggleFlag(9, 11);
    uint8_t blob[Mines::kBlobBytes];
    m.snapshot(blob);

    Mines r;
    r.load(Tier::Medium, 1u);  // deliberately a different seed
    CHECK(r.restore(blob));
    CHECK(r.seed() == 777u);
    CHECK(r.state() == m.state());
    CHECK(r.revealedCount() == m.revealedCount());
    CHECK(r.flagsPlaced() == m.flagsPlaced());
    for (int row = 0; row < m.rows(); ++row) {
      for (int col = 0; col < m.cols(); ++col) {
        assert(r.mine(col, row) == m.mine(col, row));
        assert(r.revealed(col, row) == m.revealed(col, row));
        assert(r.flagged(col, row) == m.flagged(col, row));
        assert(r.adjacent(col, row) == m.adjacent(col, row));
      }
    }
    ++gChecks;

    // A fresh board round-trips as fresh.
    Mines f;
    f.load(Tier::Hard, 9u);
    f.toggleFlag(2, 2);
    uint8_t fresh[Mines::kBlobBytes];
    f.snapshot(fresh);
    Mines g;
    g.load(Tier::Hard, 3u);
    CHECK(g.restore(fresh));
    CHECK(g.state() == Mines::State::Fresh);
    CHECK(g.flagged(2, 2));
    CHECK(g.revealedCount() == 0);

    // Geometry mismatch: the Medium blob has bits outside an Easy board.
    Mines small;
    small.load(Tier::Easy, 777u);
    CHECK(!small.restore(blob));

    // Garbage state byte and an impossible first cell are refused.
    uint8_t bad[Mines::kBlobBytes];
    m.snapshot(bad);
    bad[5] = 9;
    CHECK(!r.restore(bad));
    m.snapshot(bad);
    bad[4] = 0xFF;  // Playing with no first cell
    CHECK(!r.restore(bad));
  }

  printf("mines: all assertions passed (%d checks)\n", gChecks);
  return 0;
}
