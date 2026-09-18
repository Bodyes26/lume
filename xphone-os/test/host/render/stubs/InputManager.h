#pragma once

// Host render harness — the SDK button manager reduced to a level mask.
//
// Only the ADC ladder sampling is faked: Input.h's tap/long-press state machine
// is compiled as-is, so a simulated press travels the same edges as a real one
// (tap on RELEASE, long-press fired once at 550 ms while held, tap suppressed
// afterwards). That is what makes a harness screenshot trustworthy for input
// flows like the sudoku digit strip.

#include <cstdint>

class InputManager {
 public:
  enum : uint8_t {
    BTN_UP = 0,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_CONFIRM,
    BTN_BACK,
    BTN_POWER,
    BTN_COUNT,
  };

  // Set by the harness before each Input::update().
  static uint8_t levels;
  static bool anyEdge;

  void begin() {}
  void update() {}
  bool isPressed(uint8_t i) const { return i < BTN_COUNT && ((levels >> i) & 1u); }
  bool wasAnyPressed() const { return anyEdge; }
  unsigned long getPowerButtonHeldTime() const { return 0; }
};
