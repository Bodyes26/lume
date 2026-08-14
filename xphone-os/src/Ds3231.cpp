// DS3231 driver — see Ds3231.h for why this is not the SDK's PCF8563 Rtc lib.

#include "Ds3231.h"

#include <Arduino.h>
#include <BoardConfig.h>
#include <Wire.h>

#include "ClockStore.h"

namespace Ds3231 {
namespace {

constexpr uint8_t kAddr = 0x68;  // same address XteinkDetect fingerprints

// Register map.
constexpr uint8_t kRegSeconds = 0x00;  // 0x00-0x06: sec,min,hour,dow,date,month,year
constexpr uint8_t kRegControl = 0x0E;
constexpr uint8_t kRegStatus = 0x0F;
constexpr uint8_t kRegTempMsb = 0x11;  // 0x11-0x12

// Hours register (0x02) bits.
constexpr uint8_t kHour12Mode = 0x40;  // 1 = 12-hour mode
constexpr uint8_t kHourPm = 0x20;      // PM flag while in 12-hour mode
// Month register (0x05) bit 7 is Century; this driver always writes it clear
// (it toggles on the 99->00 year rollover, i.e. in 2100).
// Status register (0x0F) bits.
constexpr uint8_t kStatusOsf = 0x80;
constexpr uint8_t kStatusEn32kHz = 0x08;
constexpr uint8_t kStatusA2f = 0x02;
constexpr uint8_t kStatusA1f = 0x01;
// Control (0x0E): EOSC=0 (run on VBAT), BBSQW=0, CONV=0, RS=00, INTCN=1
// (INT/SQW is an alarm output, i.e. idle), A2IE=0, A1IE=0.
constexpr uint8_t kControlSafe = 0x04;

// Returns the RTC address with the shared sensor bus brought up. The X3 has one
// I2C controller and TwoWire::begin() is idempotent (same pattern as
// BatteryGauge::busAddr and Sleep.cpp::imuSleep), so this is safe per call.
uint8_t bus() {
  const auto& g = BoardConfig::ACTIVE.batteryGauge;  // SDA 20 / SCL 0 / 400 kHz
  Wire.begin(g.i2cSda, g.i2cScl, g.i2cHz);
  return kAddr;
}

bool readBytes(uint8_t reg, uint8_t* data, uint8_t n) {
  const uint8_t addr = bus();
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;  // repeated start
  if (Wire.requestFrom(addr, n, static_cast<uint8_t>(1)) < n) return false;
  for (uint8_t i = 0; i < n; ++i) data[i] = Wire.read();
  return true;
}

bool writeBytes(uint8_t reg, const uint8_t* data, uint8_t n) {
  const uint8_t addr = bus();
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.write(data, n) != n) return false;
  return Wire.endTransmission() == 0;
}

bool writeReg(uint8_t reg, uint8_t value) { return writeBytes(reg, &value, 1); }

uint8_t fromBcd(uint8_t v) { return static_cast<uint8_t>((v >> 4) * 10u + (v & 0x0Fu)); }
uint8_t toBcd(uint8_t v) { return static_cast<uint8_t>(((v / 10u) << 4) | (v % 10u)); }

bool plausible(const DateTime& dt) {
  return dt.year >= 2000 && dt.year <= 2099 && dt.month >= 1 && dt.month <= 12 && dt.day >= 1 &&
         dt.day <= 31 && dt.hour <= 23 && dt.minute <= 59 && dt.second <= 59;
}

// Clears everything in the status register we never want set, preserving OSF
// unless `clearOsf` (only true right after writing a fresh time).
bool tidyStatus(bool clearOsf) {
  uint8_t st = 0;
  if (!readBytes(kRegStatus, &st, 1)) return false;
  uint8_t next = static_cast<uint8_t>(st & ~(kStatusEn32kHz | kStatusA2f | kStatusA1f));
  if (clearOsf) next = static_cast<uint8_t>(next & ~kStatusOsf);
  if (next == st) return true;
  return writeReg(kRegStatus, next);
}

}  // namespace

Status begin() {
  uint8_t st = 0;
  if (!readBytes(kRegStatus, &st, 1)) {
    Serial.println("[lume] rtc: DS3231 did not answer on the sensor bus");
    return Status::Absent;
  }
  uint8_t ctrl = 0;
  if (readBytes(kRegControl, &ctrl, 1) && ctrl != kControlSafe) {
    writeReg(kRegControl, kControlSafe);
  }
  tidyStatus(false);

  DateTime dt;
  const Status status = read(dt);
  switch (status) {
    case Status::Ok:
      Serial.printf("[lume] rtc: DS3231 %04u-%02u-%02u %02u:%02u:%02u\n", dt.year, dt.month, dt.day,
                    dt.hour, dt.minute, dt.second);
      break;
    case Status::Invalid:
      // Either the coin/backup cell is dead or absent, or the chip has never
      // been set: the phone's time.sync stays the only authority this boot.
      Serial.println("[lume] rtc: DS3231 OSF set — time lost, waiting for time.sync");
      break;
    case Status::Absent:
      Serial.println("[lume] rtc: DS3231 registers unreadable");
      break;
  }
  return status;
}

Status read(DateTime& out) {
  uint8_t st = 0;
  if (!readBytes(kRegStatus, &st, 1)) return Status::Absent;
  uint8_t r[7];
  if (!readBytes(kRegSeconds, r, sizeof(r))) return Status::Absent;

  DateTime dt;
  dt.second = fromBcd(static_cast<uint8_t>(r[0] & 0x7F));
  dt.minute = fromBcd(static_cast<uint8_t>(r[1] & 0x7F));
  if (r[2] & kHour12Mode) {
    const uint8_t h12 = fromBcd(static_cast<uint8_t>(r[2] & 0x1F));
    dt.hour = static_cast<uint8_t>((h12 % 12u) + ((r[2] & kHourPm) ? 12u : 0u));
  } else {
    dt.hour = fromBcd(static_cast<uint8_t>(r[2] & 0x3F));
  }
  dt.day = fromBcd(static_cast<uint8_t>(r[4] & 0x3F));
  dt.month = fromBcd(static_cast<uint8_t>(r[5] & 0x1F));
  dt.year = static_cast<uint16_t>(2000u + fromBcd(r[6]));
  if (!plausible(dt)) return Status::Absent;

  out = dt;
  return (st & kStatusOsf) ? Status::Invalid : Status::Ok;
}

bool set(const DateTime& dt) {
  if (!plausible(dt)) return false;
  // Day-of-week is not used by any consumer, but the register must hold 1-7;
  // keep the same Monday=1 convention the reading stats use.
  const int32_t serial = clockSerialFromYmd(static_cast<uint32_t>(dt.year) * 10000u +
                                            static_cast<uint32_t>(dt.month) * 100u + dt.day);
  const uint8_t dow = static_cast<uint8_t>(((serial % 7) + 7 + 3) % 7 + 1);

  const uint8_t r[7] = {
      toBcd(dt.second),
      toBcd(dt.minute),
      toBcd(dt.hour),  // bit 6 clear = 24-hour mode
      dow,
      toBcd(dt.day),
      toBcd(dt.month),  // century bit clear: this driver lives in 20xx
      toBcd(static_cast<uint8_t>(dt.year - 2000u)),
  };
  if (!writeBytes(kRegSeconds, r, sizeof(r))) return false;
  // Only now is the time trustworthy, so OSF may be cleared.
  return tidyStatus(true);
}

bool oscillatorStopped(bool& stopped) {
  uint8_t st = 0;
  if (!readBytes(kRegStatus, &st, 1)) return false;
  stopped = (st & kStatusOsf) != 0;
  return true;
}

bool readTemperatureTenthsC(int16_t& out) {
  uint8_t b[2];
  if (!readBytes(kRegTempMsb, b, sizeof(b))) return false;
  // 10-bit two's complement, 0.25 °C per LSB.
  int16_t raw = static_cast<int16_t>((static_cast<uint16_t>(b[0]) << 2) | (b[1] >> 6));
  if (raw & 0x0200) raw = static_cast<int16_t>(raw - 0x0400);
  out = static_cast<int16_t>((static_cast<int32_t>(raw) * 25) / 10);
  return true;
}

}  // namespace Ds3231
