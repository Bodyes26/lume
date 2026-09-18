#include "GameStats.h"

#include <Preferences.h>
#include <cstring>

namespace {
// Shared with every other setting on purpose: a private namespace would orphan
// the records the first time this one was renamed.
constexpr const char* kPrefsNamespace = "xphone";
// NVS keys are capped at 15 characters.
constexpr const char* kRecordsKey = "gmRec";
constexpr const char* kBoardKey[games::kGameCount] = {"gmSud", "gmNon", "gmMin", "gmTrl"};

// One blob for the three records, not nine keys: a streak update is a single
// flash write. Bump on any layout change — an old blob is then refused whole
// rather than reinterpreted into a wrong streak.
constexpr uint8_t kRecordsVersion = 1;
constexpr std::size_t kRecordBytes = 8;  // streak LE16 + total LE16 + lastDay LE32
constexpr std::size_t kRecordsBytes = 1 + games::kGameCount * kRecordBytes;
// [int32 day][uint8 len][blob]
constexpr std::size_t kBoardHeader = 5;

constexpr uint16_t kCountMax = 0xFFFFu;

uint16_t bump(uint16_t v) { return (v < kCountMax) ? static_cast<uint16_t>(v + 1) : kCountMax; }

void put16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFFu);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
}

uint16_t get16(const uint8_t* p) {
  return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) | static_cast<uint16_t>(p[1] << 8));
}

void put32(uint8_t* p, int32_t v) {
  const uint32_t u = static_cast<uint32_t>(v);
  p[0] = static_cast<uint8_t>(u & 0xFFu);
  p[1] = static_cast<uint8_t>((u >> 8) & 0xFFu);
  p[2] = static_cast<uint8_t>((u >> 16) & 0xFFu);
  p[3] = static_cast<uint8_t>((u >> 24) & 0xFFu);
}

int32_t get32(const uint8_t* p) {
  const uint32_t u = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                     (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
  return static_cast<int32_t>(u);
}
}  // namespace

games::GameStats GAME_STATS;

namespace games {

void GameStats::load() {
  if (_loaded) return;
  _loaded = true;

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, /*readOnly=*/true)) return;

  uint8_t buf[kRecordsBytes] = {};
  if (prefs.getBytesLength(kRecordsKey) == kRecordsBytes &&
      prefs.getBytes(kRecordsKey, buf, sizeof(buf)) == kRecordsBytes && buf[0] == kRecordsVersion) {
    for (int i = 0; i < kGameCount; ++i) {
      const uint8_t* p = buf + 1 + i * kRecordBytes;
      _rec[i].streak = get16(p);
      _rec[i].total = get16(p + 2);
      _rec[i].lastDay = get32(p + 4);
    }
  }

  for (int i = 0; i < kGameCount; ++i) {
    uint8_t board[kBoardHeader + kMaxBlob] = {};
    const std::size_t n = prefs.getBytesLength(kBoardKey[i]);
    if (n < kBoardHeader + 1 || n > sizeof(board)) continue;
    if (prefs.getBytes(kBoardKey[i], board, sizeof(board)) != n) continue;
    const uint8_t len = board[4];
    if (len == 0 || len > kMaxBlob || kBoardHeader + len != n) continue;
    _saved[i].day = get32(board);
    _saved[i].len = len;
    memcpy(_saved[i].blob, board + kBoardHeader, len);
  }

  prefs.end();
}

const GameRecord& GameStats::record(Game g) const { return _rec[slot(g)]; }

bool GameStats::dailyDone(Game g, int32_t daySerial) const {
  // Day 0 is "clock never set", which is nobody's completed day.
  if (daySerial == 0) return false;
  return _rec[slot(g)].lastDay == daySerial;
}

void GameStats::markDailyDone(Game g, int32_t daySerial) {
  // Without a clock there is no day to attribute the win to, and inventing one
  // would let a streak grow on a device that never knew the date.
  if (daySerial == 0) return;
  if (dailyDone(g, daySerial)) return;  // restore + re-finish must not inflate anything

  GameRecord& r = _rec[slot(g)];
  r.streak = (r.lastDay != 0 && daySerial == r.lastDay + 1) ? bump(r.streak) : 1;
  r.total = bump(r.total);
  r.lastDay = daySerial;
  clearDaily(g);  // the board is finished; keeping it would resurrect a solved grid
  persistRecords();
}

void GameStats::markFreeDone(Game g) {
  _rec[slot(g)].total = bump(_rec[slot(g)].total);
  persistRecords();
}

bool GameStats::saveDaily(Game g, int32_t daySerial, const uint8_t* blob, std::size_t n) {
  if (daySerial == 0 || blob == nullptr || n == 0 || n > kMaxBlob) return false;
  Saved& s = _saved[slot(g)];
  s.day = daySerial;
  s.len = static_cast<uint8_t>(n);
  memcpy(s.blob, blob, n);
  return persistSaved(g);
}

std::size_t GameStats::loadDaily(Game g, int32_t daySerial, uint8_t* out, std::size_t cap) const {
  const Saved& s = _saved[slot(g)];
  if (daySerial == 0 || s.day != daySerial || s.len == 0) return 0;
  if (out == nullptr || cap < s.len) return 0;
  memcpy(out, s.blob, s.len);
  return s.len;
}

void GameStats::clearDaily(Game g) {
  Saved& s = _saved[slot(g)];
  s.day = 0;
  s.len = 0;
  persistSaved(g);
}

bool GameStats::persistRecords() {
  uint8_t buf[kRecordsBytes] = {};
  buf[0] = kRecordsVersion;
  for (int i = 0; i < kGameCount; ++i) {
    uint8_t* p = buf + 1 + i * kRecordBytes;
    put16(p, _rec[i].streak);
    put16(p + 2, _rec[i].total);
    put32(p + 4, _rec[i].lastDay);
  }

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, /*readOnly=*/false)) return false;
  const bool ok = prefs.putBytes(kRecordsKey, buf, sizeof(buf)) == sizeof(buf);
  prefs.end();
  return ok;
}

bool GameStats::persistSaved(Game g) {
  const Saved& s = _saved[slot(g)];
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, /*readOnly=*/false)) return false;

  bool ok;
  if (s.len == 0) {
    ok = prefs.remove(kBoardKey[slot(g)]);  // free the sector rather than store an empty board
  } else {
    uint8_t buf[kBoardHeader + kMaxBlob];
    put32(buf, s.day);
    buf[4] = s.len;
    memcpy(buf + kBoardHeader, s.blob, s.len);
    const std::size_t n = kBoardHeader + s.len;
    ok = prefs.putBytes(kBoardKey[slot(g)], buf, n) == n;
  }
  prefs.end();
  return ok;
}

}  // namespace games
