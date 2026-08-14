// Host test for xphone-os/src/Ds3231.cpp against the fake register file.
#include "ds3231_harness.h"

FakeDs3231 CHIP;
FakeSerial Serial;
FakeWire Wire;

// The driver's Arduino/Wire/BoardConfig includes resolve to the harness above.
#include "Ds3231.cpp"

#include <cassert>

namespace {

uint8_t bcd(int v) { return static_cast<uint8_t>(((v / 10) << 4) | (v % 10)); }

void loadTime(int y, int mo, int d, int h, int mi, int s, uint8_t status) {
  CHIP.reg[0x00] = bcd(s);
  CHIP.reg[0x01] = bcd(mi);
  CHIP.reg[0x02] = bcd(h);  // 24-hour: bit 6 clear
  CHIP.reg[0x03] = 1;
  CHIP.reg[0x04] = bcd(d);
  CHIP.reg[0x05] = bcd(mo);
  CHIP.reg[0x06] = bcd(y - 2000);
  CHIP.reg[0x0E] = 0x00;
  CHIP.reg[0x0F] = status;
}

void expect(bool ok, const char* what) {
  if (!ok) {
    printf("FAIL: %s\n", what);
    exit(1);
  }
}

}  // namespace

int main() {
  // 1. Absent chip: no answer must not look like a valid time.
  CHIP.present = false;
  Ds3231::DateTime dt;
  expect(Ds3231::read(dt) == Ds3231::Status::Absent, "absent chip reads Absent");
  expect(Ds3231::begin() == Ds3231::Status::Absent, "absent chip begins Absent");
  bool stopped = true;
  expect(!Ds3231::oscillatorStopped(stopped), "absent chip reports no OSF");
  int16_t tenths = 0;
  expect(!Ds3231::readTemperatureTenthsC(tenths), "absent chip reports no temperature");

  // 2. Powered chip that lost time (OSF set, e.g. dead backup cell): the
  //    registers parse, but the value must be flagged untrustworthy.
  CHIP.present = true;
  loadTime(2026, 8, 14, 9, 41, 30, 0x80 | 0x08 | 0x01);  // OSF + EN32kHz + A1F
  expect(Ds3231::begin() == Ds3231::Status::Invalid, "OSF chip begins Invalid");
  expect(CHIP.reg[0x0E] == 0x04, "begin() writes the safe control byte (INTCN, no SQW, no alarms)");
  expect((CHIP.reg[0x0F] & 0x08) == 0, "begin() clears EN32kHz");
  expect((CHIP.reg[0x0F] & 0x01) == 0, "begin() clears A1F");
  expect((CHIP.reg[0x0F] & 0x80) != 0, "begin() PRESERVES OSF — only set() may clear it");
  expect(Ds3231::oscillatorStopped(stopped) && stopped, "OSF surfaces for the About readout");

  // 3. Idempotence: a second begin() on an already-clean chip writes nothing.
  const int before = CHIP.writes;
  Ds3231::begin();
  expect(CHIP.writes == before, "begin() on a clean chip performs no register write");

  // 4. set(): BCD fields, 24-hour mode, century clear, Monday=1 weekday, OSF cleared.
  Ds3231::DateTime want;
  want.year = 2026;
  want.month = 8;
  want.day = 14;  // a Friday
  want.hour = 21;
  want.minute = 7;
  want.second = 5;
  expect(Ds3231::set(want), "set() succeeds");
  expect(CHIP.reg[0x00] == 0x05, "seconds BCD");
  expect(CHIP.reg[0x01] == 0x07, "minutes BCD");
  expect(CHIP.reg[0x02] == 0x21, "hours BCD in 24-hour mode (bit 6 clear)");
  expect((CHIP.reg[0x02] & 0x40) == 0, "12-hour bit never set");
  expect(CHIP.reg[0x03] == 5, "weekday register holds Friday as 5 (Monday=1)");
  expect(CHIP.reg[0x04] == 0x14, "date BCD");
  expect(CHIP.reg[0x05] == 0x08, "month BCD with century bit clear");
  expect(CHIP.reg[0x06] == 0x26, "year BCD");
  expect((CHIP.reg[0x0F] & 0x80) == 0, "set() clears OSF");

  expect(Ds3231::read(dt) == Ds3231::Status::Ok, "round trip reads Ok");
  expect(dt.year == 2026 && dt.month == 8 && dt.day == 14 && dt.hour == 21 && dt.minute == 7 &&
             dt.second == 5,
         "round trip returns the same instant");

  // 5. A chip left in 12-hour mode by anything else must still read correctly.
  loadTime(2026, 8, 14, 0, 0, 0, 0x00);
  CHIP.reg[0x02] = static_cast<uint8_t>(0x40 | 0x20 | bcd(7));  // 12-hour, PM, 7 -> 19:00
  expect(Ds3231::read(dt) == Ds3231::Status::Ok && dt.hour == 19, "12-hour PM converts to 19");
  CHIP.reg[0x02] = static_cast<uint8_t>(0x40 | bcd(12));  // 12 AM -> 00:00
  expect(Ds3231::read(dt) == Ds3231::Status::Ok && dt.hour == 0, "12-hour 12 AM converts to 0");
  CHIP.reg[0x02] = static_cast<uint8_t>(0x40 | 0x20 | bcd(12));  // 12 PM -> 12:00
  expect(Ds3231::read(dt) == Ds3231::Status::Ok && dt.hour == 12, "12-hour 12 PM converts to 12");

  // 6. Impossible fields (a NACKing or unpowered bus reading 0xFF) are not a date.
  loadTime(2026, 8, 14, 9, 0, 0, 0x00);
  CHIP.reg[0x05] = 0x13;  // month 13
  expect(Ds3231::read(dt) == Ds3231::Status::Absent, "month 13 is rejected");

  // 7. Temperature: 0.25 C per LSB, two's complement over 10 bits.
  loadTime(2026, 8, 14, 9, 0, 0, 0x00);
  CHIP.reg[0x11] = 25;
  CHIP.reg[0x12] = 0x40;  // 25.25 C
  expect(Ds3231::readTemperatureTenthsC(tenths) && tenths == 252, "25.25 C reads as 252 tenths");
  CHIP.reg[0x11] = 0xFF;
  CHIP.reg[0x12] = 0xC0;  // -0.25 C
  expect(Ds3231::readTemperatureTenthsC(tenths) && tenths == -2, "-0.25 C reads negative");

  puts("rtc: all assertions passed");
  return 0;
}
