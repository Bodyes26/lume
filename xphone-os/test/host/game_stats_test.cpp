// Host test for games::GameStats — records, streaks, daily completion
// idempotency, daily board snapshots, NVS persistence.

#include <cassert>
#include <cstdio>
#include <cstring>

#include <Preferences.h>
#include "games/GameStats.cpp"

using games::Game;
using games::GameRecord;
using games::GameStats;

static int gChecks = 0;
#define CHECK(cond) \
  do {              \
    ++gChecks;      \
    assert(cond);   \
  } while (0)

int main() {
  Preferences::clear();

  // Test 1: Fresh instance has zero records and no saved boards
  {
    GameStats stats;
    stats.load();
    for (int g = 0; g < games::kGameCount; g++) {
      const GameRecord& rec = stats.record(static_cast<Game>(g));
      CHECK(rec.streak == 0);
      CHECK(rec.total == 0);
      CHECK(rec.lastDay == 0);
      CHECK(!stats.dailyDone(static_cast<Game>(g), 20000));
    }
  }

  // Test 2: Daily completion and streak logic
  {
    GameStats stats;
    stats.load();

    // Day 100: first completion -> streak 1, total 1
    stats.markDailyDone(Game::Sudoku, 100);
    CHECK(stats.dailyDone(Game::Sudoku, 100));
    CHECK(!stats.dailyDone(Game::Sudoku, 101));
    CHECK(stats.record(Game::Sudoku).streak == 1);
    CHECK(stats.record(Game::Sudoku).total == 1);
    CHECK(stats.record(Game::Sudoku).lastDay == 100);

    // Same day duplicate -> idempotent
    stats.markDailyDone(Game::Sudoku, 100);
    CHECK(stats.record(Game::Sudoku).streak == 1);
    CHECK(stats.record(Game::Sudoku).total == 1);

    // Consecutive day 101 -> streak 2, total 2
    stats.markDailyDone(Game::Sudoku, 101);
    CHECK(stats.record(Game::Sudoku).streak == 2);
    CHECK(stats.record(Game::Sudoku).total == 2);
    CHECK(stats.record(Game::Sudoku).lastDay == 101);

    // 2-day gap: day 103 -> streak resets to 1, total 3
    stats.markDailyDone(Game::Sudoku, 103);
    CHECK(stats.record(Game::Sudoku).streak == 1);
    CHECK(stats.record(Game::Sudoku).total == 3);
    CHECK(stats.record(Game::Sudoku).lastDay == 103);
  }

  // Test 3: Free play completion moves total only
  {
    GameStats stats;
    stats.load();
    stats.markFreeDone(Game::Nonogram);
    CHECK(stats.record(Game::Nonogram).total == 1);
    CHECK(stats.record(Game::Nonogram).streak == 0);
    CHECK(stats.record(Game::Nonogram).lastDay == 0);
  }

  // Test 4: Daily board save and load
  {
    GameStats stats;
    stats.load();

    const uint8_t testBlob[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    CHECK(stats.saveDaily(Game::Mines, 500, testBlob, sizeof(testBlob)));

    uint8_t readBuf[32] = {};
    const std::size_t n = stats.loadDaily(Game::Mines, 500, readBuf, sizeof(readBuf));
    CHECK(n == sizeof(testBlob));
    CHECK(memcmp(readBuf, testBlob, sizeof(testBlob)) == 0);

    // Another day serial returns 0
    CHECK(stats.loadDaily(Game::Mines, 501, readBuf, sizeof(readBuf)) == 0);

    // markDailyDone clears saved board
    stats.markDailyDone(Game::Mines, 500);
    CHECK(stats.loadDaily(Game::Mines, 500, readBuf, sizeof(readBuf)) == 0);
  }

  // Test 5: Persistence across fresh GameStats instance
  {
    GameStats fresh;
    fresh.load();
    CHECK(fresh.record(Game::Sudoku).streak == 1);
    CHECK(fresh.record(Game::Sudoku).total == 3);
    CHECK(fresh.record(Game::Nonogram).total == 1);
    CHECK(fresh.record(Game::Mines).total == 1);
  }

  printf("game_stats: all assertions passed (%d checks)\n", gChecks);
  return 0;
}
