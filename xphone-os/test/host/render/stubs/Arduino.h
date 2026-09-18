#pragma once

// Host render harness — the slice of Arduino.h the drawing layer touches.
//
// The point of the harness is that Gfx.cpp, Fonts.cpp, Scene.cpp and the game
// scenes are compiled UNMODIFIED and their real output is dumped as an image, so
// a layout regression is visible without a panel. Only the platform edges are
// faked, and `millis()` is driven by the harness so auto-repeat and long-press
// paths are exercised deterministically.
//
// delay() both advances the fake clock and yields: SceneManager::waitFlushIdle()
// spins on it while the (real, threaded) flush worker finishes.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

extern unsigned long gHostMillis;

inline unsigned long millis() { return gHostMillis; }
inline void delay(unsigned long ms) {
  gHostMillis += ms;
  std::this_thread::yield();
}
inline void delayMicroseconds(unsigned int) { std::this_thread::yield(); }

// Firmware diagnostics go to stderr so the harness can keep stdout for its own
// per-frame report.
struct HostSerial {
  void begin(unsigned long) {}
  void flush() {}
  int printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int n = vfprintf(stderr, fmt, ap);
    va_end(ap);
    return n;
  }
  void print(const char* s) { fputs(s ? s : "", stderr); }
  void println(const char* s = "") { fprintf(stderr, "%s\n", s ? s : ""); }
  explicit operator bool() const { return true; }
};

extern HostSerial Serial;
