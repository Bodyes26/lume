#include "LauncherScene.h"

#include <BatteryMonitor.h>

#include <cstdio>
#include <cstring>

#include "../BatteryGauge.h"
#include "../ClockStore.h"
#include "../Fonts.h"
#include "../IconStyle.h"
#include "../StatusBar.h"
#include "../art/LauncherIcons.h"
#include "../art/LumeMark.h"
#include "../ble/CompanionBleService.h"
#include "AppScenes.h"

namespace {

// Seven focus apps across two 2x3 pages:
// Page 1 (0..5): Today, Notifications, Reminders, Block, Read, Workout
// Page 2 (6):    Games
constexpr const char* kApps[LauncherScene::APP_COUNT] = {
    L10N("Today", "Oggi"),        L10N("Notifications", "Notifiche"),
    L10N("Reminders", "Promemoria"), L10N("Block", "Focus"),
    L10N("Read", "Leggi"),        L10N("Workout", "Allenamento"),
    L10N("Games", "Giochi"),
};

void drawIcon(Gfx& gfx, const uint8_t* bitmap, const int x, const int y, const int size) {
  const int srcSize = XPhoneLauncherIconSize;
  const int rowBytes = (srcSize + 7) / 8;
  for (int row = 0; row < size; ++row) {
    const int sy = (size == srcSize) ? row : (row * srcSize) / size;
    for (int col = 0; col < size; ++col) {
      const int sx = (size == srcSize) ? col : (col * srcSize) / size;
      const uint8_t byte = bitmap[sy * rowBytes + (sx >> 3)];
      if (((byte >> (7 - (sx & 7))) & 1) == 0) gfx.drawPixel(x + col, y + row, true);
    }
  }
}

// Layout constants (2x3 grid with comfortable margins)
constexpr int kMargin = 16;       // outer margin
constexpr int kStatusH = 40;      // status bar height incl. separator
constexpr int kGap = 28;          // gap between cells (row + column)
constexpr int kSelRadius = 12;    // tile rounded-corner radius
constexpr int kSelThick = 3;      // selection border thickness
constexpr int kBoxInset = 8;      // rounded box inset inside the cell
constexpr int kTilePad = 12;      // inner padding between the box edge and icon/label
constexpr int kMaxTileSide = 186; // approved card size for 2x3 grid

BatteryMonitor& battery() {
  static BatteryMonitor mon;
  return mon;
}

}  // namespace

void LauncherScene::moveSelection(const int dCol, const int dRow) {
  int sel = _sel;
  const int prevPage = _sel / PAGE_SIZE;

  // Front Left/Right = linear PREV/NEXT with wrap across all apps & pages
  if (dCol != 0) {
    sel = (sel + dCol) % APP_COUNT;
    if (sel < 0) sel += APP_COUNT;
  }

  // Side Up/Down = vertical navigation
  if (dRow < 0) {
    if (sel >= COLS) sel -= COLS;
  }
  if (dRow > 0) {
    if (sel + COLS < APP_COUNT) {
      sel += COLS;
    } else if (sel < APP_COUNT - 1) {
      sel = APP_COUNT - 1;
    }
  }

  if (sel != _sel) {
    const int prev = _sel;
    _sel = sel;
    const int newPage = _sel / PAGE_SIZE;

    if (newPage != prevPage) {
      // Page flip: redraw the entire launcher
      markDirty();
    } else {
      // Same page: differential refresh of the two affected tiles
      XpRect dirty = cellRect(prev % PAGE_SIZE);
      dirty.unionWith(cellRect(_sel % PAGE_SIZE));
      markDirty(dirty);
    }
  }
}

XpRect LauncherScene::cellRect(const int pageIdx) const {
  if (_side <= 0) return XpRect{};
  constexpr int16_t kSlop = 6;
  const int col = pageIdx % COLS;
  const int row = pageIdx / COLS;
  const int x = _gridX + col * (_side + kGap);
  const int y = _gridY + row * (_cellH + kGap);
  return XpRect{static_cast<int16_t>(x - kSlop), static_cast<int16_t>(y - kSlop),
                static_cast<int16_t>(_side + 2 * kSlop), static_cast<int16_t>(_cellH + 2 * kSlop)};
}

void LauncherScene::handleInput(Input& in) {
  if (in.wasLongPressed(Btn::Down)) {
    showBlockDeepWork();
    return;
  }
  if (in.wasPressed(Btn::Left)) moveSelection(-1, 0);
  if (in.wasPressed(Btn::Right)) moveSelection(+1, 0);
  if (in.wasPressed(Btn::Up)) moveSelection(0, -1);
  if (in.wasPressed(Btn::Down)) moveSelection(0, +1);
  if (in.wasPressed(Btn::Confirm)) {
    const char* app = kApps[_sel];
    if (strcmp(app, L10N("Block", "Focus")) == 0) {
      showBlock();
    } else if (strcmp(app, L10N("Reminders", "Promemoria")) == 0) {
      showReminders();
    } else if (strcmp(app, L10N("Today", "Oggi")) == 0) {
      showToday();
    } else if (strcmp(app, L10N("Notifications", "Notifiche")) == 0) {
      showNotifications();
    } else if (strcmp(app, L10N("Read", "Leggi")) == 0) {
      showReader();
    } else if (strcmp(app, L10N("Workout", "Allenamento")) == 0) {
      showWorkout();
    } else if (strcmp(app, L10N("Games", "Giochi")) == 0) {
      showGames();
    }
  }
  if (in.wasPressed(Btn::Back)) showSettings();
}

const char* const* LauncherScene::softKeys() const {
  static constexpr const char* kKeys[4] = {
      L10N("SETTINGS", "IMPOSTA"), L10N("OPEN", "APRI"), L10N("PREV", "PREC"), L10N("NEXT", "SUCC")};
  return kKeys;
}

void LauncherScene::render(Gfx& gfx) {
  const int w = gfx.width();
  const int h = gfx.height();

  // --- Status bar ---
  drawLumeMark(gfx, kMargin + 12, 3, 24);
  gfx.drawText(kFontBold, kMargin + 34, 4, "lume");

  const int barH = kStatusH - 2;
  uint16_t pct = 0;
  const bool havePct = battery().readPercentageChecked(pct);
  int16_t avgMa = 0;
  const bool charging = BatteryGauge::readAvgCurrentMa(avgMa) && avgMa > 0;
  const int battLeft = StatusBar::drawBattery(gfx, w - kMargin, barH,
                                              havePct ? static_cast<int>(pct) : -1, charging);

  int clockRight = battLeft;
  if (COMPANION_BLE.isStarted()) {
    const int d = 12;
    const int dotX = battLeft - d - 10;
    const int dotY = (barH - d) / 2;
    if (COMPANION_BLE.isConnected()) {
      gfx.fillRoundedRect(dotX, dotY, d, d, d / 2, true);
    } else {
      gfx.drawRoundedRect(dotX, dotY, d, d, d / 2, 2, true);
    }
    clockRight = dotX;
  }

  char clock[16];
  if (clockFormatTime(clock, sizeof(clock))) {
    const int textW = gfx.textWidth(kFontRegular, clock);
    const int clockX = clockRight - 14 - textW;
    if (clockX > kMargin + 34 + gfx.textWidth(kFontBold, "lume") + 12) {
      gfx.drawText(kFontRegular, clockX, (barH - gfx.lineHeight(kFontRegular)) / 2 + 1, clock);
    }
  }
  gfx.fillRect(0, kStatusH - 2, w, 2, true);

  // --- 2x3 App Grid Geometry ---
  const int rows = ROWS;
  const int availTop = kStatusH;
  const int availH = h - Scene::SOFTKEY_BAR_H - availTop;
  const int sideFromW = (w - 2 * kMargin - (COLS - 1) * kGap) / COLS;
  const int sideFromH = (availH - (rows - 1) * kGap) / rows;
  int side = sideFromW < sideFromH ? sideFromW : sideFromH;
  if (side > kMaxTileSide) side = kMaxTileSide;  // capped at 186 px
  const int cellH = side;

  const int gridBlockW = COLS * side + (COLS - 1) * kGap;
  const int gridH = rows * cellH + (rows - 1) * kGap;
  const int gridX = (w - gridBlockW) / 2;
  int gridY = availTop + (availH - gridH) / 2;
  if (gridY < availTop + 4) gridY = availTop + 4;

  _gridX = static_cast<int16_t>(gridX);
  _gridY = static_cast<int16_t>(gridY);
  _side = static_cast<int16_t>(side);
  _cellH = static_cast<int16_t>(cellH);

  const int iconSize = XPhoneLauncherIconSize;  // full 104 px
  const int labelLineH = gfx.lineHeight(kFontBold);

  const int curPage = _sel / PAGE_SIZE;
  const int pageStart = curPage * PAGE_SIZE;
  const int pageEnd = (pageStart + PAGE_SIZE < APP_COUNT) ? (pageStart + PAGE_SIZE) : APP_COUNT;

  for (int i = pageStart; i < pageEnd; i++) {
    const int pageIdx = i - pageStart;
    const int col = pageIdx % COLS;
    const int row = pageIdx / COLS;
    const int cx = gridX + col * (side + kGap);
    const int cy = gridY + row * (cellH + kGap);

    const int boxX = cx + kBoxInset;
    const int boxY = cy + kBoxInset;
    const int boxSide = side - 2 * kBoxInset;
    if (i == _sel) {
      gfx.drawRoundedRect(boxX, boxY, boxSide, boxSide, kSelRadius, kSelThick, true);
    }

    const int iconAreaH = boxSide - 2 * kTilePad - labelLineH;
    const int iconY = boxY + kTilePad + (iconAreaH > iconSize ? (iconAreaH - iconSize) / 2 : 0);
    const int iconX = cx + (side - iconSize) / 2;
    if (const uint8_t* bmp = IconStyle::iconForApp(i)) {
      drawIcon(gfx, bmp, iconX, iconY, iconSize);
    }

    const XpFont& f = (i == _sel) ? kFontBold : kFontRegular;
    const int labelY = boxY + boxSide - kTilePad - labelLineH;
    gfx.drawTextCentered(f, cx + side / 2, labelY, kApps[i]);
  }

  // --- Pagination Dots ---
  constexpr int kTotalPages = (APP_COUNT + PAGE_SIZE - 1) / PAGE_SIZE;
  if (kTotalPages > 1) {
    constexpr int kDotRadius = 4;
    constexpr int kDotGap = 12;
    const int totalDotsW = kTotalPages * (2 * kDotRadius) + (kTotalPages - 1) * kDotGap;
    const int dotStartX = (w - totalDotsW) / 2 + kDotRadius;
    const int dotY = gridY + gridH + 14;

    for (int p = 0; p < kTotalPages; p++) {
      const int dx = dotStartX + p * (2 * kDotRadius + kDotGap);
      if (p == curPage) {
        gfx.fillRoundedRect(dx - kDotRadius, dotY - kDotRadius, 2 * kDotRadius, 2 * kDotRadius, kDotRadius, true);
      } else {
        gfx.drawRoundedRect(dx - kDotRadius, dotY - kDotRadius, 2 * kDotRadius, 2 * kDotRadius, kDotRadius, 1, true);
      }
    }
  }
}
