#include "ClockStore.h"

#include <Arduino.h>

#include <cstdio>

#include "LumeLocale.h"

ClockStore CLOCK_STORE;

namespace {

int32_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

void civilFromDays(int32_t z, int* y, int* m, int* d) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int yr = static_cast<int>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  *d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  *m = static_cast<int>(mp + (mp < 10 ? 3 : -9));
  *y = yr + (*m <= 2);
}

// Monday-first, matching the reading-stats weekday index (serial 0 = Thursday).
constexpr const char* kWeekdays[7] = {
    L10N("Mon", "lun"), L10N("Tue", "mar"), L10N("Wed", "mer"), L10N("Thu", "gio"),
    L10N("Fri", "ven"), L10N("Sat", "sab"), L10N("Sun", "dom"),
};
constexpr const char* kMonths[12] = {
    L10N("Jan", "gen"), L10N("Feb", "feb"), L10N("Mar", "mar"), L10N("Apr", "apr"),
    L10N("May", "mag"), L10N("Jun", "giu"), L10N("Jul", "lug"), L10N("Aug", "ago"),
    L10N("Sep", "set"), L10N("Oct", "ott"), L10N("Nov", "nov"), L10N("Dec", "dic"),
};

}  // namespace

int32_t clockSerialFromYmd(uint32_t ymd) {
  return daysFromCivil(static_cast<int>(ymd / 10000), static_cast<int>((ymd / 100) % 100),
                       static_cast<int>(ymd % 100));
}

uint32_t clockYmdFromSerial(int32_t serial) {
  int y, m, d;
  civilFromDays(serial, &y, &m, &d);
  return static_cast<uint32_t>(y) * 10000u + static_cast<uint32_t>(m) * 100u +
         static_cast<uint32_t>(d);
}

bool clockNow(uint32_t& ymd, uint16_t& minutesIntoDay) {
  if (CLOCK_STORE.day == 0 || CLOCK_STORE.anchorMs == 0) return false;
  const uint32_t elapsedMin = (millis() - CLOCK_STORE.anchorMs) / 60000u;
  const uint32_t totalMin = CLOCK_STORE.minutesIntoDay + elapsedMin;
  ymd = clockYmdFromSerial(clockSerialFromYmd(CLOCK_STORE.day) +
                           static_cast<int32_t>(totalMin / 1440u));
  minutesIntoDay = static_cast<uint16_t>(totalMin % 1440u);
  return true;
}

bool clockFormatTime(char* out, std::size_t n) {
  uint32_t ymd = 0;
  uint16_t minutes = 0;
  if (!clockNow(ymd, minutes)) return false;
  snprintf(out, n, "%02u:%02u", static_cast<unsigned>(minutes / 60u),
           static_cast<unsigned>(minutes % 60u));
  return true;
}

bool clockFormatShortDate(char* out, std::size_t n) {
  uint32_t ymd = 0;
  uint16_t minutes = 0;
  if (!clockNow(ymd, minutes)) return false;
  const int32_t serial = clockSerialFromYmd(ymd);
  const unsigned dow = static_cast<unsigned>(((serial % 7) + 7 + 3) % 7);  // 0 = Monday
  const unsigned month = (ymd / 100u) % 100u;
  if (month < 1 || month > 12) return false;
  snprintf(out, n, "%s %u %s", kWeekdays[dow], static_cast<unsigned>(ymd % 100u),
           kMonths[month - 1]);
  return true;
}
