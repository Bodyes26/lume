// Minimal Arduino/Wire/BoardConfig fake modeling a DS3231 register file, so
// xphone-os/src/Ds3231.cpp can be exercised on the host: register addressing
// with auto-increment, repeated-start reads, NACK when the chip is absent.
#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

struct FakeDs3231 {
  uint8_t reg[0x13] = {};
  bool present = true;
  int writes = 0;  // completed register writes, to prove we don't write for nothing
};
extern FakeDs3231 CHIP;

class FakeSerial {
 public:
  void printf(const char*, ...) {}
  void println(const char*) {}
  void print(const char*) {}
  void flush() {}
};
extern FakeSerial Serial;

inline void delay(uint32_t) {}
inline uint32_t millis() { return 0; }

class FakeWire {
 public:
  void begin(int, int, uint32_t) {}
  void beginTransmission(uint8_t addr) {
    _addr = addr;
    _tx.clear();
  }
  size_t write(uint8_t b) {
    _tx.push_back(b);
    return 1;
  }
  size_t write(const uint8_t* data, size_t n) {
    _tx.insert(_tx.end(), data, data + n);
    return n;
  }
  // stop = true finishes a write transaction; stop = false is the repeated
  // start the driver uses to set the read pointer.
  uint8_t endTransmission(bool stop = true) {
    if (_addr != 0x68 || !CHIP.present) return 2;  // NACK
    if (_tx.empty()) return 0;
    _ptr = _tx[0];
    if (stop && _tx.size() > 1) {
      for (size_t i = 1; i < _tx.size(); ++i) {
        const uint8_t at = static_cast<uint8_t>((_ptr + i - 1) % sizeof(CHIP.reg));
        CHIP.reg[at] = _tx[i];
      }
      ++CHIP.writes;
    }
    return 0;
  }
  int requestFrom(uint8_t addr, uint8_t n, uint8_t) {
    _rx.clear();
    if (addr != 0x68 || !CHIP.present) return 0;
    for (uint8_t i = 0; i < n; ++i) {
      _rx.push_back(CHIP.reg[static_cast<uint8_t>((_ptr + i) % sizeof(CHIP.reg))]);
    }
    _cursor = 0;
    return n;
  }
  int read() { return _cursor < _rx.size() ? _rx[_cursor++] : -1; }

 private:
  uint8_t _addr = 0;
  uint8_t _ptr = 0;
  std::vector<uint8_t> _tx;
  std::vector<uint8_t> _rx;
  size_t _cursor = 0;
};
extern FakeWire Wire;

// BoardConfig subset: only the sensor-bus pins the driver reads.
namespace BoardConfig {
struct Gauge {
  int i2cSda = 20;
  int i2cScl = 0;
  uint32_t i2cHz = 400000;
};
struct Profile {
  Gauge batteryGauge;
};
inline Profile ACTIVE;
}  // namespace BoardConfig
