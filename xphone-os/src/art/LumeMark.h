#pragma once

#include "../Gfx.h"

// Lume mark: a lowercase-style L emitting three quiet rays. Drawn from
// framebuffer primitives so the same identity scales from the 22 px launcher
// lockup to the 120 px boot mark without a generated bitmap or heap use.
inline void drawLumeMark(Gfx& gfx, const int centerX, const int topY, const int requestedSize) {
  const int unit = requestedSize >= 24 ? requestedSize / 12 : 2;
  const int extent = unit * 12;
  const int left = centerX - extent / 2;
  const int ray = unit > 2 ? unit / 2 : 1;

  // Letterform.
  gfx.fillRoundedRect(left + 2 * unit, topY + 2 * unit, 2 * unit, 7 * unit, unit, true);
  gfx.fillRoundedRect(left + 2 * unit, topY + 7 * unit, 6 * unit, 2 * unit, unit, true);

  // Light travelling out of the open corner.
  gfx.drawLine(left + 7 * unit, topY + 4 * unit, left + 10 * unit, topY + 4 * unit, ray, true);
  gfx.drawLine(left + 6 * unit, topY + 3 * unit, left + 8 * unit, topY + unit, ray, true);
  gfx.drawLine(left + 7 * unit, topY + 6 * unit, left + 9 * unit, topY + 8 * unit, ray, true);
}
