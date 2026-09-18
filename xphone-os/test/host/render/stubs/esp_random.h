#pragma once

// Host render harness — deterministic replacement for the hardware RNG, so a
// free-play screenshot is the same board every run (a diff in the rendered image
// then means a real layout change, not a new puzzle).

#include <cstdint>

extern uint32_t gHostRandom;

inline uint32_t esp_random() {
  gHostRandom = gHostRandom * 1664525u + 1013904223u;
  return gHostRandom;
}
