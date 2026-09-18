#pragma once

// Lume games — streaks, totals and the one in-progress board per game, in NVS.
//
// Two deliberate limits:
//   * NVS is written when a puzzle is FINISHED or when the device is about to
//     sleep, never per move. A move is a nibble in RAM; a flash write per move
//     would trade the panel's life for nothing.
//   * Only the DAILY board is persisted. Free play is disposable by design — the
//     thing you must not lose is the one board that cannot be regenerated
//     tomorrow.
//
// Streak semantics match the reader's (ReadingStats): the day the daily puzzle
// is completed extends the streak when the previous completion was the day
// before, otherwise it restarts at 1. Completing the same day twice is
// idempotent, so a restore + re-finish cannot inflate anything.
//
// Namespace "xphone" is shared with every other setting on purpose
// (docs/lume/CURRENT-STATE.md:58-59: renaming it would orphan pairing, reader
// position and icon style on update).

#include <cstddef>
#include <cstdint>

#include "GameTypes.h"

namespace games {

struct GameRecord {
  uint16_t streak = 0;   // consecutive days the daily puzzle was completed
  uint16_t total = 0;    // puzzles completed ever (daily + free play), saturating
  int32_t lastDay = 0;   // civil day serial of the last daily completion (0 = never)
};

class GameStats {
 public:
  // Largest snapshot of the three models, so one scratch size fits all.
  static constexpr std::size_t kMaxBlob = 64;

  // Reads the record blob and the three saved boards. Safe before NVS has ever
  // been written (first cold boot): everything stays at defaults.
  void load();

  const GameRecord& record(Game g) const;

  // Daily puzzle for `daySerial` completed: bumps total, extends or restarts the
  // streak, clears the saved board (it is finished) and persists. Idempotent
  // within the same day.
  void markDailyDone(Game g, int32_t daySerial);
  bool dailyDone(Game g, int32_t daySerial) const;
  // Free-play win: total only, no streak, no board to clear.
  void markFreeDone(Game g);

  // In-progress daily board. `blob` is the model's own snapshot; the day serial
  // it belongs to is stored alongside so a stale board is never resurrected.
  bool saveDaily(Game g, int32_t daySerial, const uint8_t* blob, std::size_t n);
  // Copies the saved board out when it belongs to `daySerial`; returns its size,
  // 0 when there is nothing usable.
  std::size_t loadDaily(Game g, int32_t daySerial, uint8_t* out, std::size_t cap) const;
  void clearDaily(Game g);

 private:
  struct Saved {
    int32_t day = 0;
    uint8_t len = 0;
    uint8_t blob[kMaxBlob] = {};
  };

  bool persistRecords();
  bool persistSaved(Game g);
  static int slot(Game g) { return static_cast<int>(g); }

  GameRecord _rec[kGameCount];
  Saved _saved[kGameCount];
  bool _loaded = false;
};

}  // namespace games

extern games::GameStats GAME_STATS;
