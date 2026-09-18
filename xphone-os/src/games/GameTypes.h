#pragma once

// Lume games — shared vocabulary for the three pastimes (sudoku, nonogram,
// minesweeper) and the deterministic seeding they share.
//
// Everything under src/games/ is PURE LOGIC: <cstdint>/<cstring> only (plus
// Preferences.h in GameStats), no Arduino, no Gfx, no heap, no FreeRTOS. That
// is what lets the whole model layer run on the host
// (test/host/{sudoku,nonogram,mines,game_stats}_test.cpp) while the scenes stay
// thin painters — the same split that makes ClockStore testable.

#include <cstdint>

namespace games {

// Free-play difficulty. The DAILY puzzle deliberately has no picker: one board
// a day for every game, always at kDailyTier, so a streak means the same thing
// on every day of the year (and so the device never asks a question at 7am).
enum class Tier : uint8_t { Easy = 0, Medium = 1, Hard = 2 };
constexpr Tier kDailyTier = Tier::Medium;
constexpr int kTierCount = 3;

enum class Game : uint8_t { Sudoku = 0, Nonogram = 1, Mines = 2, Trail = 3 };
constexpr int kGameCount = 4;
constexpr int kPuzzleGameCount = 3;

// 32-bit xorshift. Deterministic on host and target alike, which is what lets a
// minefield be rebuilt from (seed, first cell) instead of persisting its mines,
// and lets the host tests assert an exact board.
class Rng {
 public:
  explicit constexpr Rng(uint32_t seed) : _s(seed ? seed : 0x9E3779B9u) {}

  uint32_t next() {
    uint32_t x = _s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    _s = x;
    return x;
  }

  // Uniform enough for puzzle placement (the modulo bias over 2^32 is ~1e-7
  // for the bounds used here); no rejection loop, no division by zero.
  uint32_t below(uint32_t bound) { return bound ? next() % bound : 0u; }

 private:
  uint32_t _s;
};

// Puzzle of the day: hash the civil day serial (ClockStore's serial — days
// since 1970-01-01) into [0, count).
//
// A hash, NOT `serial % count`: the packs are emitted from consecutive
// generator seeds, so plain modulo would hand out neighbouring entries on
// neighbouring days — and neighbouring entries of a nonogram pack can be the
// same picture rotated. `salt` keeps the three games from moving in lockstep.
inline uint32_t dayHash(int32_t daySerial, uint32_t salt) {
  // splitmix32 finalizer: cheap, no state, well-mixed low bits (which is all
  // the modulo below looks at).
  uint32_t x = static_cast<uint32_t>(daySerial) + 0x9E3779B9u * (salt + 1u);
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return x;
}

inline uint32_t dailyIndex(int32_t daySerial, uint32_t count, uint32_t salt) {
  if (count == 0) return 0;
  return dayHash(daySerial, salt) % count;
}

// Minesweeper has no pack: its "puzzle of the day" is a seed, not an index.
// Non-zero by construction (Rng substitutes a constant for 0, which would make
// one day of the year share its field with every uninitialised board).
inline uint32_t dailySeed(int32_t daySerial, uint32_t salt) {
  return dayHash(daySerial, salt) | 1u;
}

// Salts for dailyIndex — one per game, fixed forever (changing one reshuffles
// that game's whole calendar, which would break nothing but every "I already
// did today's" expectation).
constexpr uint32_t kSaltSudoku = 1u;
constexpr uint32_t kSaltNonogram = 2u;
constexpr uint32_t kSaltMines = 3u;

}  // namespace games
