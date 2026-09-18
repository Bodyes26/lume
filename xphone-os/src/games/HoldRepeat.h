#pragma once

// Lume games — auto-repeat for a held direction button.
//
// Input.h reports a TAP on release and a long-press exactly once, with NO
// repeat while a button stays down (Input.h:151-158). That is right for lists
// of ten rows and wrong for a 15x15 grid: crossing it would cost fifteen
// separate presses. This is the missing piece, kept out of the input layer on
// purpose — only the game scenes want it, and only for the four direction
// buttons.
//
// Contract (one instance per direction, fed once per loop tick):
//   steps = rpt.tick(in.isPressed(btn), millis());
// Returns the number of cursor steps to apply this tick: 0 or 1. The first step
// still comes from the ordinary TAP (so a single press behaves exactly like
// every other scene); repeats start only after kDelayMs of hold and then fire
// every kPeriodMs.
//
// Because the tap arrives on RELEASE, a hold that already auto-repeated would
// also deliver one final tap. consumeTap() reports that pending suppression, so
// a held traverse does not overshoot by one cell.
//
// Pure logic: `now` is injected, so this is host-testable and carries no
// Arduino dependency.

#include <cstdint>

namespace games {

class HoldRepeat {
 public:
  // Long enough that a deliberate single press never repeats (Input's own
  // long-press threshold is 550 ms), short enough that a traverse feels held.
  static constexpr uint32_t kDelayMs = 600;
  // One step per panel window: a partial-window flush is the real floor here,
  // so repeating faster only queues frames the scene manager coalesces away
  // (SceneManager::renderIfDirty defers while a flush is in flight).
  static constexpr uint32_t kPeriodMs = 220;

  // Returns 1 when the caller should advance one step, else 0.
  uint8_t tick(bool down, uint32_t now) {
    if (!down) {
      _downSince = 0;
      _nextMs = 0;
      return 0;
    }
    if (_downSince == 0) {  // press edge: arm, do not step (the tap will)
      _downSince = now ? now : 1;
      _nextMs = _downSince + kDelayMs;
      return 0;
    }
    if (static_cast<uint32_t>(now - _nextMs) < 0x80000000u) {  // rollover-safe now >= _nextMs
      _nextMs = now + kPeriodMs;
      _tapPending = true;  // the release will still deliver a tap; swallow it
      return 1;
    }
    return 0;
  }

  // True once if this hold auto-repeated, meaning the tap that arrives on
  // release must be ignored.
  bool consumeTap() {
    const bool pending = _tapPending;
    _tapPending = false;
    return pending;
  }

 private:
  uint32_t _downSince = 0;
  uint32_t _nextMs = 0;
  bool _tapPending = false;
};

}  // namespace games
