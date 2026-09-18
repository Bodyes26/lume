#include "MinesScene.h"

#include <Arduino.h>
#include <esp_random.h>

#include <cstdio>

#include "../ClockStore.h"
#include "../Fonts.h"
#include "../games/GameStats.h"
#include "AppScenes.h"

namespace {

using State = games::Mines::State;

const char* tierLabel(const games::Tier t) {
  switch (t) {
    case games::Tier::Easy: return L10N("Easy", "Facile");
    case games::Tier::Medium: return L10N("Medium", "Medio");
    default: return L10N("Hard", "Difficile");
  }
}

}  // namespace

// --- Lifecycle -----------------------------------------------------------------

void MinesScene::open(const games::Tier tier, const bool daily) {
  GAME_STATS.load();  // idempotent; the games menu may not have run yet on a wake

  uint32_t ymd = 0;
  uint16_t mins = 0;
  _day = clockNow(ymd, mins) ? clockSerialFromYmd(ymd) : 0;
  // No day serial means no calendar: a "daily" board would have no identity to
  // persist under and no streak to extend, so it degrades to free play.
  _daily = daily && _day != 0;

  const games::Tier t = _daily ? games::kDailyTier : tier;
  // Non-zero seeds only: 0 is the uninitialised board's seed, and Rng maps it to
  // a constant — one day (or one free field) sharing a fixed layout.
  const uint32_t seed = _daily ? games::dailySeed(_day, games::kSaltMines) : (esp_random() | 1u);
  _model.load(t, seed);

  _col = _model.cols() / 2;
  _row = _model.rows() / 2;
  _scored = false;
  _persisted = false;
  _boomCol = _boomRow = -1;
  // A direction held while the scene changed must not auto-repeat into the new
  // board (the press edge is re-armed on the next tick).
  _rptUp = games::HoldRepeat{};
  _rptDown = games::HoldRepeat{};
  _rptLeft = games::HoldRepeat{};
  _rptRight = games::HoldRepeat{};

  if (_daily) {
    uint8_t blob[games::Mines::kBlobBytes];
    if (GAME_STATS.loadDaily(games::Game::Mines, _day, blob, sizeof blob) == sizeof blob) {
      // restore() refuses a blob from another geometry; on refusal the fresh
      // board it left alone is exactly today's field, so nothing to undo.
      if (_model.restore(blob)) {
        if (_model.state() == State::Lost) {
          // The blob does not name the fatal mine — it is the only mine the
          // player ever managed to reveal.
          for (int r = 0; r < _model.rows() && _boomCol < 0; r++) {
            for (int c = 0; c < _model.cols(); c++) {
              if (_model.revealed(c, r) && _model.mine(c, r)) {
                _boomCol = static_cast<int8_t>(c);
                _boomRow = static_cast<int8_t>(r);
                break;
              }
            }
          }
        }
        // A restored terminal board is already the one in NVS, and a restored
        // win cannot be scored twice.
        if (_model.state() != State::Playing && _model.state() != State::Fresh) {
          _scored = true;
          _persisted = true;
        }
      }
    }
  }
  markDirty();
}

void MinesScene::onExit() { persistDaily(); }

void MinesScene::persistDaily() {
  if (!_daily || _day == 0 || _persisted) return;
  const State st = _model.state();
  // Won: markDailyDone already cleared the slot. Fresh: the field is not laid
  // yet, so the daily seed still regenerates this exact board — merely opening
  // the app is not worth a flash write.
  if (st == State::Won || st == State::Fresh) return;
  uint8_t blob[games::Mines::kBlobBytes];
  _model.snapshot(blob);
  _persisted = GAME_STATS.saveDaily(games::Game::Mines, _day, blob, sizeof blob);
}

const char* const* MinesScene::softKeys() const {
  static constexpr const char* kPlay[4] = {L10N("BACK", "INDIETRO"), L10N("OPEN", "APRI"), L10N("PREV", "PREC"),
                                           L10N("NEXT", "SUCC")};
  static constexpr const char* kAgain[4] = {L10N("BACK", "INDIETRO"), L10N("NEW", "NUOVO"), nullptr, nullptr};
  static constexpr const char* kDone[4] = {L10N("BACK", "INDIETRO"), nullptr, nullptr, nullptr};
  if (playing()) return kPlay;
  // A finished DAILY board is finished for the day: offering "NEW" there would
  // promise a second field that this game cannot hand out.
  return _daily ? kDone : kAgain;
}

uint8_t MinesScene::longPressSlots() const {
  // Confirm long-press = flag, so the dot only belongs on that tab while there
  // is something to flag.
  return playing() ? 0x01 | 0x02 : 0x01;
}

// --- Geometry ------------------------------------------------------------------

int MinesScene::cellSize() const {
  // Sized per shape so the widest board still fits 528 px with margins and ends
  // above the status line: 8x52=416, 10x48=480, 12x40=480.
  switch (_model.cols()) {
    case 8: return 52;
    case 10: return 48;
    default: return 40;
  }
}

int MinesScene::gridX() const { return (_wCache - _model.cols() * cellSize()) / 2; }

int MinesScene::statusY() const { return kGridY + _model.rows() * cellSize() + 16; }

XpRect MinesScene::cellRect(const int col, const int row) const {
  if (_wCache <= 0) return XpRect{};  // before the first paint: full-panel dirty
  const int cell = cellSize();
  // Padded by the cursor ring, which is drawn 2 px outside the cell.
  return XpRect{static_cast<int16_t>(gridX() + col * cell - 3), static_cast<int16_t>(kGridY + row * cell - 3),
                static_cast<int16_t>(cell + 6), static_cast<int16_t>(cell + 6)};
}

XpRect MinesScene::statusRect() const {
  if (_wCache <= 0) return XpRect{};
  const int top = statusY() - 2;
  // Both lines: kFontRegular status + the kFontSmall daily/tier caption.
  return XpRect{0, static_cast<int16_t>(top), _wCache, static_cast<int16_t>(29 + 2 + 24 + 4)};
}

// --- Input ---------------------------------------------------------------------

void MinesScene::handleInput(Input& in) {
  if (in.wasPressed(Btn::Back)) {
    showGames();
    return;
  }

  if (!playing()) {
    // Free play only: same tier, brand new random field.
    if (!_daily && in.wasPressed(Btn::Confirm)) open(_model.tier(), false);
    return;
  }

  // Auto-repeat runs on the level, the first step still comes from the ordinary
  // tap; consumeTap() swallows the tap the release of a repeating hold delivers,
  // which would otherwise overshoot by one cell.
  const uint32_t now = millis();
  int dCol = 0, dRow = 0;
  dRow -= _rptUp.tick(in.isPressed(Btn::Up), now);
  dRow += _rptDown.tick(in.isPressed(Btn::Down), now);
  dCol -= _rptLeft.tick(in.isPressed(Btn::Left), now);
  dCol += _rptRight.tick(in.isPressed(Btn::Right), now);
  if (in.wasPressed(Btn::Up) && !_rptUp.consumeTap()) dRow--;
  if (in.wasPressed(Btn::Down) && !_rptDown.consumeTap()) dRow++;
  if (in.wasPressed(Btn::Left) && !_rptLeft.consumeTap()) dCol--;
  if (in.wasPressed(Btn::Right) && !_rptRight.consumeTap()) dCol++;
  if (dCol != 0 || dRow != 0) {
    moveCursor(dCol, dRow);
    return;
  }

  // Long-press is delivered while the button is still down and consumes the
  // hold (Input.h:151-158), so flagging can never also reveal the cell.
  if (in.wasLongPressed(Btn::Confirm)) {
    doFlag();
    return;
  }
  if (in.wasPressed(Btn::Confirm)) doReveal();
}

void MinesScene::moveCursor(const int dCol, const int dRow) {
  const int cols = _model.cols();
  const int rows = _model.rows();
  const int nc = (_col + dCol + cols) % cols;
  const int nr = (_row + dRow + rows) % rows;
  if (nc == _col && nr == _row) return;
  XpRect r = cellRect(_col, _row);
  r.unionWith(cellRect(nc, nr));
  _col = nc;
  _row = nr;
  if (r.empty()) {
    markDirty();
  } else {
    markDirty(r);
  }
}

void MinesScene::doReveal() {
  uint8_t before[games::Mines::kMaskBytes];
  captureRevealed(before);

  _model.reveal(_col, _row);

  if (_model.state() == State::Lost && _boomCol < 0) {
    _boomCol = static_cast<int8_t>(_col);
    _boomRow = static_cast<int8_t>(_row);
  }
  if (!playing()) {
    // Win or loss turns over every remaining cell (mines appear, the status line
    // changes): this is the one move that earns a full-panel repaint.
    _persisted = false;
    finishIfOver();
    markDirty();
    return;
  }

  const XpRect r = revealedDiffRect(before);
  if (r.empty()) return;  // flagged or already open: the model made this a no-op
  _persisted = false;
  // The status line is NOT unioned in: a reveal changes neither the mine total
  // nor the flag count, and stretching the window down to the status band would
  // triple the flushed area for identical pixels.
  markDirty(r);
}

void MinesScene::doFlag() {
  const bool was = _model.flagged(_col, _row);
  _model.toggleFlag(_col, _row);
  if (_model.flagged(_col, _row) == was) return;  // revealed cell: model refused
  _persisted = false;
  XpRect r = cellRect(_col, _row);
  r.unionWith(statusRect());  // the flag counter moved
  if (r.empty()) {
    markDirty();
  } else {
    markDirty(r);
  }
}

void MinesScene::finishIfOver() {
  if (_scored) return;
  const State st = _model.state();
  if (st == State::Won) {
    _scored = true;
    if (_daily) {
      GAME_STATS.markDailyDone(games::Game::Mines, _day);
      _persisted = true;  // markDailyDone cleared the saved board
    } else {
      GAME_STATS.markFreeDone(games::Game::Mines);
    }
    return;
  }
  if (st == State::Lost) {
    _scored = true;
    // Today's field is the same all day, so the loss is final: write the wreck
    // out NOW instead of trusting onExit, or a battery pull would quietly hand
    // out a second attempt at the same board.
    persistDaily();
  }
}

void MinesScene::captureRevealed(uint8_t* mask) const {
  for (int i = 0; i < games::Mines::kMaskBytes; i++) mask[i] = 0;
  for (int r = 0; r < _model.rows(); r++) {
    for (int c = 0; c < _model.cols(); c++) {
      if (!_model.revealed(c, r)) continue;
      const int i = r * _model.cols() + c;
      mask[i >> 3] |= static_cast<uint8_t>(1u << (i & 7));
    }
  }
}

XpRect MinesScene::revealedDiffRect(const uint8_t* before) const {
  XpRect r{};
  for (int row = 0; row < _model.rows(); row++) {
    for (int col = 0; col < _model.cols(); col++) {
      const int i = row * _model.cols() + col;
      const bool was = (before[i >> 3] >> (i & 7)) & 1u;
      if (was == _model.revealed(col, row)) continue;
      r.unionWith(cellRect(col, row));
    }
  }
  return r;
}

// --- Cell art ------------------------------------------------------------------

void MinesScene::drawCovered(Gfx& gfx, const int x, const int y, const int cell) const {
  gfx.drawRect(x, y, cell, cell, 1, true);
  // Sparse dither, not a fill: a board that starts as a wall of ink burns the
  // panel, needs a full flush to clear and reads as noise rather than as "not
  // opened yet".
  for (int dy = 4; dy < cell - 4; dy += 4) {
    for (int dx = 4; dx < cell - 4; dx += 4) gfx.drawPixel(x + dx, y + dy, true);
  }
}

void MinesScene::drawFlag(Gfx& gfx, const int x, const int y, const int cell) const {
  const int inset = cell / 6;
  const int poleX = x + cell / 3;
  const int top = y + inset;
  const int bottom = y + cell - inset;
  gfx.fillRect(poleX, top, 2, bottom - top, true);

  // Pennant: solid, one 1 px scanline per row. Three outline drawLine() calls
  // leave a hollow core at a 40 px cell (the thickest line Gfx offers is still
  // an outline), and a hollow flag reads as a stray arrow next to the digits.
  const int fh = (bottom - top) / 2;
  const int fw = fh;
  const int half = fh / 2;
  for (int i = 0; i <= fh; i++) {
    const int d = i <= half ? i : fh - i;
    const int len = half > 0 ? (fw * d) / half : fw;
    if (len > 0) gfx.fillRect(poleX + 2, top + i, len, 1, true);
  }
}

void MinesScene::drawMine(Gfx& gfx, const int x, const int y, const int cell, const bool fatal) const {
  const int cx = x + cell / 2;
  const int cy = y + cell / 2;
  const int r = cell / 5;
  // Gfx has no circle: a rounded rect with r == half its side IS the disc.
  gfx.fillRoundedRect(cx - r, cy - r, 2 * r, 2 * r, r, true);
  const int in = (r * 7) / 10;   // r/sqrt2 — start on the disc's diagonal edge
  const int out = in + r / 2 + 3;
  gfx.drawLine(cx - in, cy - in, cx - out, cy - out, 2, true);
  gfx.drawLine(cx + in, cy - in, cx + out, cy - out, 2, true);
  gfx.drawLine(cx - in, cy + in, cx - out, cy + out, 2, true);
  gfx.drawLine(cx + in, cy + in, cx + out, cy + out, 2, true);
  if (!fatal) return;
  // The one that ended the game: an X across the whole cell, so a post-mortem
  // board says which mine it was and not just where they all were.
  const int e = 5;
  gfx.drawLine(x + e, y + e, x + cell - e, y + cell - e, 2, true);
  gfx.drawLine(x + cell - e, y + e, x + e, y + cell - e, 2, true);
}

void MinesScene::drawCell(Gfx& gfx, const int col, const int row) const {
  const int cell = cellSize();
  const int x = gridX() + col * cell;
  const int y = kGridY + row * cell;
  // Mines are shown on a LOSS only. After a win the leftovers are the player's
  // own flags, and turning them into mines would overwrite the result they just
  // earned with the answer key.
  const bool bust = _model.state() == State::Lost;

  if (_model.revealed(col, row)) {
    if (_model.mine(col, row)) {
      drawMine(gfx, x, y, cell, col == _boomCol && row == _boomRow);
      return;
    }
    const int n = _model.adjacent(col, row);
    if (n <= 0) return;  // revealed empty: plain paper, the cheapest cell there is
    const char digit[2] = {static_cast<char>('0' + n), '\0'};
    gfx.drawTextCentered(kFontBold, x + cell / 2, y + (cell - gfx.lineHeight(kFontBold)) / 2, digit);
    return;
  }

  if (_model.flagged(col, row)) {
    drawCovered(gfx, x, y, cell);
    drawFlag(gfx, x, y, cell);
    return;
  }
  if (bust && _model.mine(col, row)) {
    drawMine(gfx, x, y, cell, false);
    return;
  }
  drawCovered(gfx, x, y, cell);
}

// --- Render --------------------------------------------------------------------

void MinesScene::render(Gfx& gfx) {
  const int w = gfx.width();
  _wCache = static_cast<int16_t>(w);
  _hCache = static_cast<int16_t>(gfx.height());

  // --- Header -----------------------------------------------------------------
  gfx.drawText(kFontBold, kMarginX, 8, L10N("Mines", "Campo minato"));
  char note[40];
  snprintf(note, sizeof note, "%s · %dx%d", _daily ? L10N("daily", "giornaliero") : tierLabel(_model.tier()),
           _model.cols(), _model.rows());
  gfx.drawText(kFontSmall, w - kMarginX - gfx.textWidth(kFontSmall, note), 12, note);
  gfx.fillRect(0, kHeaderH - 2, w, 2, true);

  // --- Board ------------------------------------------------------------------
  for (int row = 0; row < _model.rows(); row++) {
    for (int col = 0; col < _model.cols(); col++) drawCell(gfx, col, row);
  }
  // Cursor last: the ring sits 2 px OUTSIDE its cell, so any neighbour drawn
  // afterwards would clip it with its own border.
  {
    const int cell = cellSize();
    gfx.drawRoundedRect(gridX() + _col * cell - 2, kGridY + _row * cell - 2, cell + 4, cell + 4, 6, 3, true);
  }

  // --- Status -----------------------------------------------------------------
  const State st = _model.state();
  const int sy = statusY();
  char line[64];
  if (st == State::Won) {
    snprintf(line, sizeof line, "%s", L10N("Cleared!", "Bonificato!"));
  } else if (st == State::Lost) {
    snprintf(line, sizeof line, "%s", L10N("Boom.", "Bum."));
  } else {
    snprintf(line, sizeof line, L10N("mines %d  ·  flags %d", "mine %d  ·  bandiere %d"), _model.mines(),
             _model.flagsPlaced());
  }
  gfx.drawTextCentered(kFontRegular, w / 2, sy, line);

  const int subY = sy + gfx.lineHeight(kFontRegular) + 2;
  if (!_daily) {
    gfx.drawTextCentered(kFontSmall, w / 2, subY, tierLabel(_model.tier()));
  } else if (st == State::Won) {
    snprintf(line, sizeof line, L10N("streak %u days", "serie %u giorni"),
             static_cast<unsigned>(GAME_STATS.record(games::Game::Mines).streak));
    gfx.drawTextCentered(kFontSmall, w / 2, subY, line);
  } else if (st == State::Lost) {
    // Today's board is the same for everyone all day, so there is no retry to
    // offer — say so instead of leaving the player hunting for one.
    gfx.drawTextCentered(kFontSmall, w / 2, subY, L10N("today's field is over", "il campo di oggi finisce qui"));
  }
}
