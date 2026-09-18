#include "NonogramScene.h"

#include <esp_random.h>
#include <cstdio>
#include <cstring>

#include "../ClockStore.h"
#include "../Fonts.h"
#include "../LumeLocale.h"
#include "../games/GameStats.h"
#include "AppScenes.h"

namespace {

constexpr int kMarginX = 20;

int32_t currentDaySerial() {
  uint32_t ymd = 0;
  uint16_t mins = 0;
  return clockNow(ymd, mins) ? clockSerialFromYmd(ymd) : 0;
}

}  // namespace

void NonogramScene::open(games::Tier tier, bool daily) {
  _day = currentDaySerial();
  _daily = daily && (_day != 0);
  _scored = false;
  _persisted = false;

  const games::Tier targetTier = _daily ? games::kDailyTier : tier;
  const uint32_t count = games::Nonogram::packCount(targetTier);
  uint32_t index = 0;
  if (_daily) {
    index = games::dailyIndex(_day, count, games::kSaltNonogram);
  } else if (count > 0) {
    index = esp_random() % count;
  }

  _model.load(targetTier, index);
  _col = 0;
  _row = 0;

  if (_daily) {
    uint8_t blob[games::Nonogram::kBlobBytes];
    const std::size_t n = GAME_STATS.loadDaily(games::Game::Nonogram, _day, blob, sizeof(blob));
    if (n == sizeof(blob) && _model.restore(blob)) {
      _persisted = true;
    }
  }

  if (_model.solved()) {
    _scored = true;
  }

  markDirty();
}

void NonogramScene::persistDaily() {
  if (!_daily || _persisted) return;
  uint8_t blob[games::Nonogram::kBlobBytes];
  _model.snapshot(blob);
  GAME_STATS.saveDaily(games::Game::Nonogram, _day, blob, sizeof(blob));
  _persisted = true;
}

void NonogramScene::onExit() {
  persistDaily();
}

int NonogramScene::cellSize() const {
  const int s = _model.side();
  if (s == 10) return 40;
  if (s == 12) return 33;
  return 26;  // side 15
}

int NonogramScene::blockW() const {
  return kLeftGutterW + cellSize() * _model.side();
}

int NonogramScene::gridX() const {
  if (_wCache <= 0) return (528 - blockW()) / 2 + kLeftGutterW;
  return (_wCache - blockW()) / 2 + kLeftGutterW;
}

int NonogramScene::statusY() const {
  return kGridY + cellSize() * _model.side() + 20;
}

XpRect NonogramScene::cellRect(int col, int row) const {
  if (_wCache <= 0) return XpRect{};
  const int cs = cellSize();
  const int gx = gridX();
  const int x = gx + col * cs;
  const int y = kGridY + row * cs;
  return XpRect{static_cast<int16_t>(x - 3), static_cast<int16_t>(y - 3),
                static_cast<int16_t>(cs + 6), static_cast<int16_t>(cs + 6)};
}

XpRect NonogramScene::rowIndicatorRect(int row) const {
  if (_wCache <= 0) return XpRect{};
  const int x0 = gridX() - kLeftGutterW;
  const int cs = cellSize();
  const int y = kGridY + row * cs + (cs - 5) / 2;
  return XpRect{static_cast<int16_t>(x0 + 4), static_cast<int16_t>(y), 8, 8};
}

XpRect NonogramScene::colIndicatorRect(int col) const {
  if (_wCache <= 0) return XpRect{};
  const int gx = gridX();
  const int cs = cellSize();
  const int x = gx + col * cs + (cs - 5) / 2;
  return XpRect{static_cast<int16_t>(x), 54, 8, 8};
}

XpRect NonogramScene::statusRect() const {
  if (_wCache <= 0) return XpRect{};
  const int y = statusY();
  return XpRect{kMarginX, static_cast<int16_t>(y - 4),
                static_cast<int16_t>(_wCache - 2 * kMarginX), 64};
}

void NonogramScene::moveCursor(int dCol, int dRow) {
  const int s = _model.side();
  const int prevCol = _col;
  const int prevRow = _row;

  if (dCol != 0) {
    _col = (_col + dCol) % s;
    if (_col < 0) _col += s;
  }
  if (dRow != 0) {
    _row = (_row + dRow) % s;
    if (_row < 0) _row += s;
  }

  if (_col != prevCol || _row != prevRow) {
    XpRect r = cellRect(prevCol, prevRow);
    r.unionWith(cellRect(_col, _row));
    markDirty(r);
  }
}

void NonogramScene::toggleFill() {
  if (_model.solved()) return;
  _model.cycleFill(_col, _row);
  _persisted = false;
  checkFinish();

  if (_model.solved()) {
    markDirty();  // full panel for solve
  } else {
    XpRect r = cellRect(_col, _row);
    r.unionWith(rowIndicatorRect(_row));
    r.unionWith(colIndicatorRect(_col));
    r.unionWith(statusRect());
    markDirty(r);
  }
}

void NonogramScene::toggleMark() {
  if (_model.solved()) return;
  _model.cycleMark(_col, _row);
  _persisted = false;

  XpRect r = cellRect(_col, _row);
  r.unionWith(statusRect());
  markDirty(r);
}

void NonogramScene::checkFinish() {
  if (_scored || !_model.solved()) return;
  _scored = true;
  if (_daily) {
    GAME_STATS.markDailyDone(games::Game::Nonogram, _day);
    _persisted = true;
  } else {
    GAME_STATS.markFreeDone(games::Game::Nonogram);
  }
}

void NonogramScene::handleInput(Input& in) {
  const uint32_t now = millis();

  // Tick the four hold-repeat engines
  const uint8_t repUp = _rptUp.tick(in.isPressed(Btn::Up), now);
  const uint8_t repDown = _rptDown.tick(in.isPressed(Btn::Down), now);
  const uint8_t repLeft = _rptLeft.tick(in.isPressed(Btn::Left), now);
  const uint8_t repRight = _rptRight.tick(in.isPressed(Btn::Right), now);

  if (repUp) moveCursor(0, -1);
  if (repDown) moveCursor(0, 1);
  if (repLeft) moveCursor(-1, 0);
  if (repRight) moveCursor(1, 0);

  const bool swallowUp = _rptUp.consumeTap();
  const bool swallowDown = _rptDown.consumeTap();
  const bool swallowLeft = _rptLeft.consumeTap();
  const bool swallowRight = _rptRight.consumeTap();

  if (in.wasPressed(Btn::Up) && !swallowUp) moveCursor(0, -1);
  if (in.wasPressed(Btn::Down) && !swallowDown) moveCursor(0, 1);
  if (in.wasPressed(Btn::Left) && !swallowLeft) moveCursor(-1, 0);
  if (in.wasPressed(Btn::Right) && !swallowRight) moveCursor(1, 0);

  if (in.wasLongPressed(Btn::Confirm)) {
    toggleMark();
    return;
  }

  if (in.wasPressed(Btn::Confirm)) {
    if (_model.solved()) {
      if (!_daily) open(_model.tier(), false);
    } else {
      toggleFill();
    }
    return;
  }

  if (in.wasPressed(Btn::Back)) {
    showGames();
  }
}

const char* const* NonogramScene::softKeys() const {
  static constexpr const char* kPlaying[4] = {
      L10N("BACK", "INDIETRO"), L10N("FILL", "RIEMPI"), L10N("PREV", "PREC"), L10N("NEXT", "SUCC")};
  static constexpr const char* kSolvedDaily[4] = {
      L10N("BACK", "INDIETRO"), nullptr, nullptr, nullptr};
  static constexpr const char* kSolvedFree[4] = {
      L10N("BACK", "INDIETRO"), L10N("NEW", "NUOVO"), nullptr, nullptr};

  if (_model.solved()) {
    return _daily ? kSolvedDaily : kSolvedFree;
  }
  return kPlaying;
}

uint8_t NonogramScene::longPressSlots() const {
  return _model.solved() ? 0x01 : (0x01 | 0x02);
}

void NonogramScene::drawGrid(Gfx& gfx, int gx, int gy, int cs, int s) const {
  const int totalW = cs * s;
  for (int i = 0; i <= s; i++) {
    const int thick = (i == 0 || i == s || (i % 5 == 0)) ? 2 : 1;
    // Vertical line
    const int vx = gx + i * cs - (thick == 2 ? 1 : 0);
    gfx.fillRect(vx, gy, thick, totalW, true);
    // Horizontal line
    const int hy = gy + i * cs - (thick == 2 ? 1 : 0);
    gfx.fillRect(gx, hy, totalW, thick, true);
  }
}

void NonogramScene::drawClues(Gfx& gfx, int gx, int gy, int cs, int s) const {
  const int lineH = gfx.lineHeight(kFontSmall);

  // Row clues: right-aligned ending at gx - 8
  for (int r = 0; r < s; r++) {
    uint8_t clues[games::Nonogram::kMaxClues];
    const int count = _model.rowClues(r, clues);
    const int rowY = gy + r * cs + (cs - lineH) / 2 + 1;
    int rx = gx - 8;

    if (count == 0) {
      const int tw = gfx.textWidth(kFontSmall, "0");
      gfx.drawText(kFontSmall, rx - tw, rowY, "0", true);
    } else {
      for (int k = count - 1; k >= 0; k--) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", clues[k]);
        const int tw = gfx.textWidth(kFontSmall, buf);
        gfx.drawText(kFontSmall, rx - tw, rowY, buf, true);
        rx -= (tw + 6);
      }
    }
  }

  // Column clues: stacked upward ending at gy - 6
  for (int c = 0; c < s; c++) {
    uint8_t clues[games::Nonogram::kMaxClues];
    const int count = _model.colClues(c, clues);
    const int colCenterX = gx + c * cs + cs / 2;
    int by = gy - 6;

    if (count == 0) {
      const int tw = gfx.textWidth(kFontSmall, "0");
      gfx.drawText(kFontSmall, colCenterX - tw / 2, by - lineH, "0", true);
    } else {
      for (int k = count - 1; k >= 0; k--) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", clues[k]);
        const int tw = gfx.textWidth(kFontSmall, buf);
        gfx.drawText(kFontSmall, colCenterX - tw / 2, by - lineH, buf, true);
        by -= 20;
      }
    }
  }
}

void NonogramScene::drawIndicators(Gfx& gfx, int x0, int gx, int gy, int cs, int s) const {
  // Row completion indicator
  for (int r = 0; r < s; r++) {
    if (_model.rowDone(r)) {
      const int y = gy + r * cs + (cs - 5) / 2;
      gfx.fillRect(x0 + 4, y, 5, 5, true);
    }
  }

  // Column completion indicator
  for (int c = 0; c < s; c++) {
    if (_model.colDone(c)) {
      const int x = gx + c * cs + (cs - 5) / 2;
      gfx.fillRect(x, 54, 5, 5, true);
    }
  }
}

void NonogramScene::drawCells(Gfx& gfx, int gx, int gy, int cs, int s) const {
  const int pad = (cs <= 26) ? 5 : (cs <= 33 ? 7 : 9);

  for (int r = 0; r < s; r++) {
    for (int c = 0; c < s; c++) {
      const int cx = gx + c * cs;
      const int cy = gy + r * cs;
      const games::NonoCell state = _model.at(c, r);

      if (state == games::NonoCell::Fill) {
        gfx.fillRect(cx + 1, cy + 1, cs - 2, cs - 2, true);
      } else if (state == games::NonoCell::Mark) {
        gfx.drawLine(cx + pad, cy + pad, cx + cs - pad, cy + cs - pad, 2, true);
        gfx.drawLine(cx + cs - pad, cy + pad, cx + pad, cy + cs - pad, 2, true);
      }
    }
  }

  // Draw cursor ring
  const int curX = gx + _col * cs;
  const int curY = gy + _row * cs;
  gfx.drawRoundedRect(curX - 2, curY - 2, cs + 4, cs + 4, 6, 3, true);
}

void NonogramScene::render(Gfx& gfx) {
  _wCache = static_cast<int16_t>(gfx.width());
  _hCache = static_cast<int16_t>(gfx.height());
  const int w = gfx.width();

  // --- Header ---
  char right[32];
  const char* tierLabel = _daily ? L10N("daily", "del giorno")
                                 : (_model.tier() == games::Tier::Easy ? L10N("easy", "facile")
                                                                       : (_model.tier() == games::Tier::Medium ? L10N("medium", "medio")
                                                                                                               : L10N("hard", "difficile")));
  snprintf(right, sizeof(right), "%s  %dx%d", tierLabel, _model.side(), _model.side());

  gfx.drawText(kFontBold, kMarginX, 8, L10N("Nonogram", "Nonogram"));
  const int rightW = gfx.textWidth(kFontSmall, right);
  gfx.drawText(kFontSmall, w - kMarginX - rightW, 12, right);
  gfx.fillRect(0, kHeaderH - 2, w, 2, true);

  // --- Grid & Clues ---
  const int cs = cellSize();
  const int s = _model.side();
  const int gx = gridX();
  const int x0 = gx - kLeftGutterW;

  drawClues(gfx, gx, kGridY, cs, s);
  drawIndicators(gfx, x0, gx, kGridY, cs, s);
  drawGrid(gfx, gx, kGridY, cs, s);
  drawCells(gfx, gx, kGridY, cs, s);

  // --- Status line ---
  const int sy = statusY();
  if (_model.solved()) {
    const char* picName = _model.name();
    if (picName && picName[0] != '\0') {
      gfx.drawTextCentered(kFontBold, w / 2, sy, picName);
    } else {
      gfx.drawTextCentered(kFontBold, w / 2, sy, L10N("Solved!", "Completato!"));
    }

    if (_daily) {
      const auto& rec = GAME_STATS.record(games::Game::Nonogram);
      char streakBuf[48];
      snprintf(streakBuf, sizeof(streakBuf), L10N("streak %u days", "serie %u giorni"),
               static_cast<unsigned>(rec.streak));
      gfx.drawTextCentered(kFontSmall, w / 2, sy + gfx.lineHeight(kFontBold) + 2, streakBuf);
    }
  } else {
    char countBuf[48];
    snprintf(countBuf, sizeof(countBuf), L10N("%d of %d filled", "%d su %d riempite"),
             _model.filled(), _model.targetCount());
    gfx.drawTextCentered(kFontRegular, w / 2, sy, countBuf);
  }
}
