// Host test for xphone-os/src/ClockStore.cpp: civil-date round trip, weekday
// naming, midnight rollover from the millis() anchor. Arduino millis() is
// stubbed so the anchor arithmetic is exercised deterministically.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

static uint32_t g_millis = 0;
uint32_t millis() { return g_millis; }

#include "ClockStore.cpp"

static void expectDate(uint32_t day, uint16_t minutes, uint32_t nowMs, const char* wantTime,
                       const char* wantDate) {
  CLOCK_STORE.day = day;
  CLOCK_STORE.minutesIntoDay = minutes;
  CLOCK_STORE.anchorMs = 1000;
  g_millis = nowMs;
  char t[16], d[20];
  assert(clockFormatTime(t, sizeof(t)));
  assert(clockFormatShortDate(d, sizeof(d)));
  if (strcmp(t, wantTime) != 0 || strcmp(d, wantDate) != 0) {
    printf("FAIL day=%u min=%u now=%u -> '%s' '%s' (want '%s' '%s')\n", day, minutes, nowMs, t, d,
           wantTime, wantDate);
    exit(1);
  }
}

int main() {
  // Unset clock: every reader must be able to tell.
  CLOCK_STORE = ClockStore{};
  uint32_t ymd = 0;
  uint16_t min = 0;
  assert(!clockNow(ymd, min));
  char buf[24];
  assert(!clockFormatTime(buf, sizeof(buf)));
  assert(!clockFormatShortDate(buf, sizeof(buf)));

  // Serial round trip over a century, including every leap-year rule.
  for (int32_t s = -1000; s < 40000; ++s) {
    assert(clockSerialFromYmd(clockYmdFromSerial(s)) == s);
  }
  assert(clockSerialFromYmd(19700101) == 0);
  assert(clockYmdFromSerial(0) == 19700101);

  // Anchor at 09:40, no elapsed time.
  expectDate(20260814, 9 * 60 + 40, 1000, "09:40", L10N("Fri 14 Aug", "ven 14 ago"));
  // 20 minutes of uptime later.
  expectDate(20260814, 9 * 60 + 40, 1000 + 20 * 60000, "10:00", L10N("Fri 14 Aug", "ven 14 ago"));
  // Across midnight: 23:50 + 25 min -> next day 00:15.
  expectDate(20260814, 23 * 60 + 50, 1000 + 25 * 60000, "00:15", L10N("Sat 15 Aug", "sab 15 ago"));
  // Across a month AND a leap day: 2028-02-28 23:59 + 2 min.
  expectDate(20280228, 23 * 60 + 59, 1000 + 2 * 60000, "00:01", L10N("Tue 29 Feb", "mar 29 feb"));
  // Across a year: 2026-12-31 23:59 + 1 min.
  expectDate(20261231, 23 * 60 + 59, 1000 + 60000, "00:00", L10N("Fri 1 Jan", "ven 1 gen"));
  // Many days of uptime with no sync (rollover by whole days).
  expectDate(20260814, 12 * 60, 1000 + 3u * 1440u * 60000u, "12:00", L10N("Mon 17 Aug", "lun 17 ago"));

  puts("clock: all assertions passed");
  return 0;
}
