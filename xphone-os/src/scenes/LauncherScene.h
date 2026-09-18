#pragma once

// Lume — launcher: status bar + paginated 2x3 app grid.
//
// Page size is 6 (2 columns x 3 rows), so the tiles stay at their full
// comfortable 186x186 size and the 104 px icon masters fit without
// downsampling. Apps 0..5 live on page 1, app 6 (Giochi) on page 2.
// PREV / NEXT cycle through all apps and flip pages automatically.

#include "../Scene.h"

class LauncherScene : public Scene {
 public:
  void handleInput(Input& in) override;
  void render(Gfx& gfx) override;
  const char* const* softKeys() const override;  // [gear] / OPEN / PREV / NEXT
  uint8_t softKeyIconMask() const override { return 0x01; }  // Settings = gear tab

  static constexpr int COLS = 2;
  static constexpr int ROWS = 3;
  static constexpr int PAGE_SIZE = COLS * ROWS;  // 6
  static constexpr int APP_COUNT = 7;

 private:
  void moveSelection(int dCol, int dRow);
  // Logical rect of grid cell on the CURRENT page (tile + label, small slop).
  XpRect cellRect(int i) const;

  int _sel = 0;  // persists across scene switches (static instance)

  int16_t _gridX = 0, _gridY = 0, _side = 0, _cellH = 0;
};
