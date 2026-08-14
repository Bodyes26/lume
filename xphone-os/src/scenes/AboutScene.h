#pragma once

// xphone-os M1 — About: version, heap, boot time, panel dims.
//
// Two pages (PREC/SUCC soft keys, front Left/Right or top-edge Up/Down): the
// rows no longer fit one screen. Page 1 = identity, boot, wake, clock, heap +
// refresh instrumentation; page 2 = power, battery, gauge, radio + BLE/ANCS,
// notifications, stack, and the power-button hint. onEnter() resets to page 1.

#include "../Scene.h"

class AboutScene : public Scene {
 public:
  void onEnter() override;
  void handleInput(Input& in) override;
  void render(Gfx& gfx) override;
  const char* const* softKeys() const override;  // BACK / - / PREV / NEXT

 private:
  static constexpr uint8_t kPageCount = 2;

  // Both take the content cursor by value (y = top of the first row, below the
  // title rule) — each page owns its own cursor, nothing flows back.
  void renderIdentity(Gfx& gfx, int x, int y);
  void renderDiagnostics(Gfx& gfx, int x, int y);

  uint8_t _page = 0;
};
