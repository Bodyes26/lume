// DS3231 real-time clock (X3 only) — the hardware clock the rest of the
// firmware used to assume did not exist.
//
// The chip has always been on the board: XteinkDetect probes it at I2C 0x68 as
// one third of the X3 fingerprint (freeink-sdk XteinkDetect.cpp). It sits on
// the same single I2C controller as the BQ27220 gauge and the QMI8658 IMU
// (SDA 20 / SCL 0 / 400 kHz, BoardConfig XTEINK_X3 batteryGauge), so this
// driver follows BatteryGauge.cpp / Sleep.cpp::imuSleep and re-runs the
// idempotent TwoWire::begin() per operation instead of owning the bus. It must
// run AFTER freeink::detectXteinkIsX3(), which ends with Wire.end().
//
// Why not the SDK's Rtc lib: it speaks PCF8563 only. Its REG_TIME is 0x02
// (DS3231 time starts at 0x00, so every field would be two registers off with
// no I2C error), its validity bit is VL in the seconds register (the DS3231
// keeps validity in status 0x0F bit 7, OSF), and it writes 0x0D as CLKOUT
// (0x0D is Alarm2 day/date here). It is also not in lib_deps and compiles to
// stubs on this build. See docs/lume/12-rtc-e-solo-x3.md §1.3.
//
// Registers used (DS3231 datasheet): 0x00-0x06 time/date BCD, 0x0E control,
// 0x0F status (OSF bit 7), 0x11-0x12 temperature. Alarms are deliberately left
// disabled: INT/SQW is not routed to any GPIO on the X3, and on battery the
// GPIO13 latch cuts MCU power entirely, so a timed wake is not reachable in
// software (docs/lume/12-rtc-e-solo-x3.md §1.8).
#pragma once

#include <cstddef>
#include <cstdint>

namespace Ds3231 {

struct DateTime {
  uint16_t year = 2000;  // full year; the driver always writes century = 0
  uint8_t month = 1;     // 1-12
  uint8_t day = 1;       // 1-31
  uint8_t hour = 0;      // 0-23 (24-hour; a 12-hour register is converted)
  uint8_t minute = 0;    // 0-59
  uint8_t second = 0;    // 0-59
};

enum class Status : uint8_t {
  Absent,   // no answer on the bus, or fields that cannot be a date
  Invalid,  // chip answered, but OSF says the oscillator stopped: do not trust
  Ok,       // running clock, time is trustworthy
};

// Brings the bus up, forces the safe control byte (oscillator on, no square
// wave, no alarm interrupts), clears EN32kHz and the stale alarm flags, and
// returns the state of the clock. Safe to call more than once.
Status begin();

// Current time. `out` is filled whenever the registers parse as a plausible
// date — including the Invalid case, so diagnostics can show what the chip
// holds — but only Ok means the value is trustworthy.
Status read(DateTime& out);

// Sets the clock (24-hour mode, century 0) and clears OSF, so the next read
// reports Ok. Returns false on any I2C error or out-of-range field.
bool set(const DateTime& dt);

// Oscillator Stop Flag (status 0x0F bit 7): true = the clock lost time since
// it was last set. False return means the chip did not answer.
bool oscillatorStopped(bool& stopped);

// Die temperature in tenths of a degree Celsius (0.25 °C resolution).
bool readTemperatureTenthsC(int16_t& out);

}  // namespace Ds3231
