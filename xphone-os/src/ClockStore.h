#pragma once

// The device's notion of local time, and the one place civil-date math lives.
//
// Two sources feed it, in this order:
//   1. Boot: the DS3231 on the sensor bus (Ds3231.h) — X3 hardware always had
//      one, the firmware just never read it. main.cpp seeds this store before
//      the first paint, so date attribution, streaks and the on-glass clock
//      work with no phone in range and survive the power-button wake (which is
//      a full chip reset and wipes software time).
//   2. The iPhone's "time.sync" card, which stays the AUTHORITY: only the phone
//      knows the time zone and DST. CompanionBleService writes the RTC back
//      when the two disagree by more than a minute, so the chip is the flywheel
//      and the phone is the reference.
//
// The anchor is (day, minutesIntoDay) stamped at anchorMs; readers roll it
// forward with millis(). Every source that sets the date MUST re-stamp anchorMs
// in the same breath — a new date on a stale anchor is silently off by the
// uptime between them, and reader stats derive the day from exactly this pair.
//
// Write paths: firstConnectMs from the NimBLE host task (markConnected), the
// RTC seed from boot(), sync fields from the main-loop card parse. All fields
// are single 32/16/8-bit aligned scalars — atomic on the C3, no mutex.

#include <cstddef>
#include <cstdint>

struct ClockStore {
  uint32_t firstConnectMs = 0;    // millis() at first BLE connect since boot (0 = none yet)
  uint32_t anchorMs = 0;          // millis() when day/minutesIntoDay were stamped (0 = never)
  uint32_t day = 0;               // local yyyymmdd of the anchor
  uint16_t minutesIntoDay = 0;    // local minutes since midnight at the anchor
  bool fromRtc = false;           // anchor came from the DS3231, no phone sync yet
  uint32_t firstPhoneSyncMs = 0;  // millis() of the first time.sync (wake->date latency)
};

extern ClockStore CLOCK_STORE;

// Civil-calendar conversions (Howard Hinnant's algorithms) so day arithmetic
// crosses month/year boundaries correctly without <ctime>. Serial 0 is
// 1970-01-01, a Thursday.
int32_t clockSerialFromYmd(uint32_t ymd);
uint32_t clockYmdFromSerial(int32_t serial);

// Local time now: the anchor rolled forward by uptime. False (and outputs
// untouched) when no source has set the clock since boot.
bool clockNow(uint32_t& ymd, uint16_t& minutesIntoDay);

// "09:41" — 24-hour, the product format. False when the clock is unset.
bool clockFormatTime(char* out, std::size_t n);

// Localized short date, "gio 14 ago" / "Thu 14 Aug". False when unset.
bool clockFormatShortDate(char* out, std::size_t n);
