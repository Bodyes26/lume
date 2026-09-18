#include "GamesScene.h"

#include <cstdio>

#include "../ClockStore.h"
#include "../Fonts.h"
#include "../games/GameStats.h"
#include "AppScenes.h"

// Chrome matches the other app cards (SettingsScene.cpp:32-34): 20px side
// padding, a 46px header band closed by a 2px rule, content from y=46 down to
// the soft-key bar. The two views differ on purpose:
//   * the GAME list uses tall rows with a rounded outline selector, like the
//     launcher, because each row carries two lines (name + today's state) and
//     an inverted bar would drown them;
//   * the MODE list is CrossPoint's inverted selector bar (label left, value
//     right), like Settings — four one-line rows read as a menu.
namespace {
constexpr int kMarginX = 20;
constexpr int kHeaderH = 46;
constexpr int kRowPad = 16;      // mode row height = lineHeight(bold) + kRowPad
constexpr int kListTop = 62;     // first row's text top, both views
constexpr int kListRowH = 75;    // game-row pitch (two text lines + breathing room)
constexpr int kListBoxH = 71;    // selector outline height (pitch minus the gap)
constexpr int kHintGap = 14;     // list bottom -> hint line

const char* gameName(const games::Game g) {
  switch (g) {
    case games::Game::Sudoku: return "Sudoku";
    case games::Game::Nonogram: return "Nonogram";
    case games::Game::Mines: return L10N("Mines", "Campo minato");
    case games::Game::Trail: return L10N("Adventures", "Avventure");
  }
  return "";
}

// One CrossPoint-style mode row: inverted full-width bar when selected, label
// left / value right (SettingsScene.cpp:288-300). The value is kFontSmall, not
// Settings' kFontRegular, because "orologio non impostato" in 12pt would run
// into the label.
void drawRow(Gfx& gfx, const int y, const int rowH, const char* label, const char* value,
             const bool selected) {
  if (selected) gfx.fillRect(0, y, gfx.width(), rowH, true);
  if (value && value[0]) {
    gfx.drawText(kFontSmall, gfx.width() - kMarginX - gfx.textWidth(kFontSmall, value),
                 y + (rowH - gfx.lineHeight(kFontSmall)) / 2, value, !selected);
  }
  gfx.drawText(kFontBold, kMarginX, y + (rowH - gfx.lineHeight(kFontBold)) / 2, label, !selected);
}

// Explanatory line under a list. Centered when it fits between the margins,
// wrapped otherwise: the Italian strings run a good 15% longer than the English
// ones, and a centered line that overflows loses ink off BOTH edges.
void drawHint(Gfx& gfx, const int y, const char* text) {
  const int maxW = gfx.width() - 2 * kMarginX;
  if (gfx.textWidth(kFontSmall, text) <= maxW) {
    gfx.drawTextCentered(kFontSmall, gfx.width() / 2, y, text);
  } else {
    gfx.drawTextWrapped(kFontSmall, kMarginX, y, text, maxW, 2);
  }
}
}  // namespace

void GamesScene::onEnter() {
  _view = View::List;
  // Idempotent: only the first call reads NVS, later ones return immediately —
  // so re-entering the app after every game costs nothing.
  GAME_STATS.load();

  // Day serial, or 0 when the clock was never set (no DS3231 seed and no phone
  // yet). Recomputed on every entry so a device that gets its first time.sync
  // while sitting in the launcher finds a daily board waiting.
  uint32_t ymd = 0;
  uint16_t mins = 0;
  _day = clockNow(ymd, mins) ? clockSerialFromYmd(ymd) : 0;
}

const char* const* GamesScene::softKeys() const {
  static constexpr const char* kKeys[4] = {L10N("BACK", "INDIETRO"), L10N("OPEN", "APRI"), L10N("UP", "SU"), L10N("DOWN", "GIÙ")};
  return kKeys;
}

XpRect GamesScene::listRowRect(const int index) const {
  if (_wCache <= 0) return XpRect{};
  return XpRect{8, static_cast<int16_t>(kListTop + index * kListRowH - 6),
                static_cast<int16_t>(_wCache - 16), static_cast<int16_t>(kListBoxH)};
}

XpRect GamesScene::modeRowRect(const int index) const {
  if (_wCache <= 0) return XpRect{};
  // The row height needs lineHeight(kFontBold), and there is no Gfx here —
  // Gfx::lineHeight just returns the font's own advance, so read it directly.
  const int rowH = static_cast<int>(kFontBold.lineAdvance) + kRowPad;
  return XpRect{0, static_cast<int16_t>(kListTop + index * rowH), _wCache,
                static_cast<int16_t>(rowH)};
}

void GamesScene::enterMode(const games::Game g) {
  _game = g;
  _view = View::Mode;
  _modeSel = firstModeRow();  // never park the cursor on a daily row nobody can open
  markDirty();
}

void GamesScene::moveSel(const int delta) {
  const bool list = _view == View::List;
  int& sel = list ? _listSel : _modeSel;
  const int low = list ? 0 : firstModeRow();
  const int high = (list ? games::kGameCount : kModeCount) - 1;

  int next = sel + delta;
  if (next < low) next = low;
  if (next > high) next = high;  // clamped, never wrapped: 3-4 rows all fit on glass
  if (next == sel) return;

  const int prev = sel;
  sel = next;
  // Two rows changed ink, nothing else did — a full-panel FAST flush here would
  // cost 450ms for 150 px of movement.
  markDirty(list ? listRowRect(prev) : modeRowRect(prev));
  markDirty(list ? listRowRect(next) : modeRowRect(next));
}

void GamesScene::launchSelected() {
  const bool daily = _modeSel == 0;
  if (daily && _day == 0) return;  // no day serial, no board to attribute it to
  const games::Tier tier = daily ? games::kDailyTier : static_cast<games::Tier>(_modeSel - 1);
  switch (_game) {
    case games::Game::Sudoku: showSudoku(tier, daily); break;
    case games::Game::Nonogram: showNonogram(tier, daily); break;
    case games::Game::Mines: showMines(tier, daily); break;
  }
}

void GamesScene::handleInput(Input& in) {
  if (in.wasPressed(Btn::Back)) {
    if (_view == View::Mode) {
      _view = View::List;
      markDirty();
      return;
    }
    showLauncher();
    return;
  }

  if (in.wasPressed(Btn::Up) || in.wasPressed(Btn::Left)) moveSel(-1);
  if (in.wasPressed(Btn::Down) || in.wasPressed(Btn::Right)) moveSel(+1);

  if (in.wasPressed(Btn::Confirm)) {
    if (_view == View::List) {
      if (static_cast<games::Game>(_listSel) == games::Game::Trail) {
        showTrailList();
      } else {
        enterMode(static_cast<games::Game>(_listSel));
      }
    } else {
      launchSelected();  // switches scenes: touch nothing after this
    }
  }
}
// --- rendering ---------------------------------------------------------------

void GamesScene::drawHeader(Gfx& gfx, const char* title, const char* right) {
  gfx.drawText(kFontBold, kMarginX, 8, title);
  if (right && right[0]) {
    gfx.drawText(kFontSmall, gfx.width() - kMarginX - gfx.textWidth(kFontSmall, right), 12, right);
  }
  gfx.fillRect(0, kHeaderH - 2, gfx.width(), 2, true);
}

void GamesScene::renderList(Gfx& gfx) {
  char date[24];
  drawHeader(gfx, L10N("Games", "Giochi"),
             clockFormatShortDate(date, sizeof(date)) ? date : nullptr);

  for (int i = 0; i < games::kGameCount; i++) {
    const games::Game game = static_cast<games::Game>(i);
    const int rowY = kListTop + i * kListRowH;

    // Selector FIRST: drawRoundedRect hollows its border with an inner white
    // round-rect (Gfx.h:85-86), which would erase text drawn before it.
    if (i == _listSel) {
      gfx.drawRoundedRect(8, rowY - 6, gfx.width() - 16, kListBoxH, 12, 3, true);
    }

    gfx.drawText(kFontBold, kMarginX, rowY, gameName(game));

    if (game == games::Game::Trail) {
      gfx.drawText(kFontSmall, kMarginX, rowY + gfx.lineHeight(kFontBold) + 2,
                   L10N("interactive stories", "storie interattive"));
      continue;
    }

    const games::GameRecord& rec = GAME_STATS.record(game);
    if (rec.streak > 0) {
      char streak[32];
      snprintf(streak, sizeof(streak), L10N("streak %u", "serie %u"),
               static_cast<unsigned>(rec.streak));
      gfx.drawText(kFontSmall, gfx.width() - kMarginX - gfx.textWidth(kFontSmall, streak),
                   rowY + (gfx.lineHeight(kFontBold) - gfx.lineHeight(kFontSmall)) / 2, streak);
    }

    const char* status = L10N("clock not set", "orologio non impostato");
    if (_day != 0) {
      status = GAME_STATS.dailyDone(game, _day) ? L10N("today: done", "oggi: fatto")
                                               : L10N("today: to do", "oggi: da fare");
    }
    gfx.drawText(kFontSmall, kMarginX, rowY + gfx.lineHeight(kFontBold) + 2, status);
  }
  drawHint(gfx, kListTop + games::kGameCount * kListRowH + kHintGap,
           L10N("One board a day. Free play has three levels.",
                "Una tavola al giorno. Il gioco libero ha tre livelli."));
}

void GamesScene::renderMode(Gfx& gfx) {
  drawHeader(gfx, gameName(_game), nullptr);

  static constexpr const char* kLabels[kModeCount] = {L10N("Daily", "Del giorno"), L10N("Easy", "Facile"), L10N("Medium", "Medio"), L10N("Hard", "Difficile")};

  const int rowH = gfx.lineHeight(kFontBold) + kRowPad;
  for (int i = 0; i < kModeCount; i++) {
    const char* value = nullptr;
    if (i == 0) {
      value = L10N("clock not set", "orologio non impostato");
      if (_day != 0) {
        value = GAME_STATS.dailyDone(_game, _day) ? L10N("done", "fatto") : L10N("to do", "da fare");
      }
    }
    drawRow(gfx, kListTop + i * rowH, rowH, kLabels[i], value, i == _modeSel);
  }

  drawHint(gfx, kListTop + kModeCount * rowH + kHintGap,
           L10N("Same board for everyone, all day. Keeps the streak.",
                "La stessa tavola per tutto il giorno. Tiene la serie."));
}

void GamesScene::render(Gfx& gfx) {
  _wCache = static_cast<int16_t>(gfx.width());  // captured for the row dirty rects
  if (_view == View::List) {
    renderList(gfx);
  } else {
    renderMode(gfx);
  }
}
