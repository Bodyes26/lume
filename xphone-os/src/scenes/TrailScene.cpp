#include "TrailScene.h"

#include <cstdio>

#include "../Fonts.h"
#include "../LumeLocale.h"
#include "AppScenes.h"

namespace {
constexpr int kHeaderY = 8;
constexpr int kTextTop = 86;
constexpr int kChoiceGap = 8;
}  // namespace

bool TrailScene::open(const uint8_t* storyData, uint32_t storySize, bool resumeSaved) {
  _storyLoaded = _engine.loadStory(storyData, storySize);
  if (!_storyLoaded) return false;

  _choiceSel = 0;

  if (resumeSaved) {
    const auto* hdr = _engine.header();
    trail::TrailSave save;
    if (hdr && trail::TRAIL_SAVES.load(trail::storyHash(hdr->id), save)) {
      if (_engine.restoreGame(save)) {
        markDirty();
        return true;
      }
    }
  }

  bool ok = _engine.startNewGame();
  markDirty();
  return ok;
}

void TrailScene::onExit() {
  persistSave();
}

void TrailScene::persistSave() {
  if (!_storyLoaded) return;
  trail::TrailSave save;
  _engine.createSave(save);
  trail::TRAIL_SAVES.save(save);
}

const char* const* TrailScene::softKeys() const {
  if (_engine.state() == trail::EngineState::Minigame) {
    static const char* const kMinigameKeys[] = {
      L10N("BACK", "INDIETRO"),
      "OK",
      L10N("LEFT", "SX"),
      L10N("RIGHT", "DX")
    };
    return kMinigameKeys;
  }

  static const char* const kDefaultKeys[] = {
    L10N("BACK", "INDIETRO"),
    "OK",
    L10N("UP", "SU"),
    L10N("DOWN", "GIU")
  };
  return kDefaultKeys;
}

void TrailScene::moveChoiceSel(int delta) {
  const auto& node = _engine.currentNode();
  if (node.choiceCount <= 0) return;

  int next = _choiceSel + delta;
  if (next < 0) next = node.choiceCount - 1;
  if (next >= node.choiceCount) next = 0;

  _choiceSel = next;
  markDirty();
}

void TrailScene::handleInput(Input& in) {
  if (in.wasPressed(Btn::Back)) {
    if (_engine.state() == trail::EngineState::Minigame) {
      _engine.updateMinigameKey(trail::Key::Back);
      markDirty();
      return;
    }
    persistSave();
    showGames();
    return;
  }

  if (_engine.state() == trail::EngineState::Minigame) {
    if (in.wasPressed(Btn::Up))    { _engine.updateMinigameKey(trail::Key::Up); markDirty(); }
    if (in.wasPressed(Btn::Down))  { _engine.updateMinigameKey(trail::Key::Down); markDirty(); }
    if (in.wasPressed(Btn::Left))  { _engine.updateMinigameKey(trail::Key::Left); markDirty(); }
    if (in.wasPressed(Btn::Right)) { _engine.updateMinigameKey(trail::Key::Right); markDirty(); }
    if (in.wasPressed(Btn::Confirm)) { _engine.updateMinigameKey(trail::Key::Confirm); markDirty(); }
    return;
  }

  if (_engine.state() == trail::EngineState::ChapterEnded ||
      _engine.state() == trail::EngineState::GameOver ||
      _engine.state() == trail::EngineState::StoryWon) {
    if (in.wasPressed(Btn::Confirm)) {
      if (_engine.state() == trail::EngineState::ChapterEnded) {
        _engine.advanceAuto();
        _choiceSel = 0;
        markDirty();
      } else {
        persistSave();
        showGames();
      }
    }
    return;
  }

  // Narrative mode navigation: Up/Left = previous choice, Down/Right = next choice
  if (in.wasPressed(Btn::Up) || in.wasPressed(Btn::Left))     moveChoiceSel(-1);
  if (in.wasPressed(Btn::Down) || in.wasPressed(Btn::Right)) moveChoiceSel(+1);

  if (in.wasPressed(Btn::Confirm)) {
    if (_engine.isChoiceAvailable(_choiceSel)) {
      _engine.makeChoice(_choiceSel);
      _choiceSel = 0;
      persistSave();
      markDirty();
    }
  }
}

void TrailScene::drawHeader(Gfx& gfx, const char* title, const char* subtitle) {
  const int w = gfx.width();
  gfx.drawText(kFontBold, kMarginX, kHeaderY, title ? title : "Trail");
  if (subtitle && subtitle[0]) {
    int sw = gfx.textWidth(kFontSmall, subtitle);
    gfx.drawText(kFontSmall, w - kMarginX - sw, kHeaderY + 4, subtitle);
  }
  gfx.fillRect(0, kHeaderH - 2, w, 2, true);
}

void TrailScene::drawResourceBand(Gfx& gfx, int y) {
  const auto* meta = _engine.meta();
  if (!meta || meta->resourceCount == 0) return;

  const int w = gfx.width();
  int x = kMarginX;
  int availW = w - 2 * kMarginX;
  int itemW = availW / (meta->resourceCount > 4 ? 4 : meta->resourceCount);

  for (int i = 0; i < meta->resourceCount && i < 4; ++i) {
    const auto& res = meta->resources[i];
    int16_t val = _engine.resource(i);

    char label[32];
    snprintf(label, sizeof(label), "%s: %d", res.name, val);
    gfx.drawText(kFontSmall, x, y, label);

    // Mini bar below label
    int barW = itemW - 12;
    int barH = 4;
    int barY = y + gfx.lineHeight(kFontSmall) + 2;
    gfx.drawRect(x, barY, barW, barH, 1, true);

    int fillW = res.max > 0 ? (val * (barW - 2)) / res.max : 0;
    if (fillW > barW - 2) fillW = barW - 2;
    if (fillW > 0) {
      gfx.fillRect(x + 1, barY + 1, fillW, barH - 2, true);
    }

    x += itemW;
  }

  gfx.fillRect(kMarginX, y + kResBarH - 2, availW, 1, true);
}

void TrailScene::renderNarrative(Gfx& gfx) {
  const int w = gfx.width();
  const auto* hdr = _engine.header();

  char sub[48];
  snprintf(sub, sizeof(sub), "%s %d · %s %d",
           L10N("Cap.", "Cap."), _engine.currentChapter() + 1,
           L10N("Day", "Giorno"), _engine.day());
  drawHeader(gfx, hdr ? hdr->title : "Avventura", sub);

  drawResourceBand(gfx, kHeaderH + 4);

  // Narrative Text
  const char* text = _engine.nodeText();
  int textY = kTextTop + kResBarH;
  int maxW = w - 2 * kMarginX;

  int linesDrawn = gfx.drawTextWrapped(kFontRegular, kMarginX, textY, text, maxW, 8);
  int choicesTop = textY + linesDrawn * gfx.lineHeight(kFontRegular) + 16;

  // Choices
  const auto& node = _engine.currentNode();
  int choiceY = choicesTop;
  int choiceRowH = gfx.lineHeight(kFontBold) + 8;

  for (int i = 0; i < node.choiceCount; ++i) {
    const char* label = _engine.choiceLabel(i);
    bool available = _engine.isChoiceAvailable(i);
    bool selected = (i == _choiceSel);

    if (selected) {
      gfx.drawRoundedRect(kMarginX - 6, choiceY - 2, maxW + 12, choiceRowH, 6, 2, true);
      gfx.drawText(kFontBold, kMarginX + 4, choiceY + 2, "▸");
    }

    char displayLabel[80];
    if (!available) {
      snprintf(displayLabel, sizeof(displayLabel), "(%s)", label);
    } else {
      snprintf(displayLabel, sizeof(displayLabel), "%s", label);
    }

    gfx.drawText(available ? kFontBold : kFontSmall, kMarginX + 24, choiceY + 2, displayLabel, available);
    choiceY += choiceRowH + kChoiceGap;
  }
}

void TrailScene::renderMinigame(Gfx& gfx) {
  const auto& vm = _engine.vm();
  const int w = gfx.width();

  const char* titleStr = vm.string(vm.titleId());
  drawHeader(gfx, titleStr && titleStr[0] ? titleStr : L10N("Minigame", "Minigioco"), nullptr);

  // Draw Grid
  int gw = vm.gridW();
  int gh = vm.gridH();
  int cellSize = 36;
  if (gw > 8 || gh > 8) cellSize = 28;
  if (gw > 12 || gh > 12) cellSize = 22;

  int gridPixW = gw * cellSize;
  int gridPixH = gh * cellSize;
  int startX = (w - gridPixW) / 2;
  int startY = kHeaderH + 16;

  for (int r = 0; r < gh; ++r) {
    for (int c = 0; c < gw; ++c) {
      int cx = startX + c * cellSize;
      int cy = startY + r * cellSize;

      gfx.drawRect(cx, cy, cellSize, cellSize, 1, true);

      uint8_t cellVal = vm.cell(c, r);
      char ch = vm.cellChar(cellVal);
      char str[2] = {ch, '\0'};

      int tw = gfx.textWidth(kFontBold, str);
      int tx = cx + (cellSize - tw) / 2;
      int ty = cy + (cellSize - gfx.lineHeight(kFontBold)) / 2;
      gfx.drawText(kFontBold, tx, ty, str);
    }
  }

  // Draw Cursor ring
  int curCellX = startX + vm.curX() * cellSize;
  int curCellY = startY + vm.curY() * cellSize;
  gfx.drawRoundedRect(curCellX - 2, curCellY - 2, cellSize + 4, cellSize + 4, 4, 2, true);

  // Status & message
  int msgY = startY + gridPixH + 16;
  const char* textStr = vm.string(vm.textId());
  if (textStr && textStr[0]) {
    gfx.drawTextCentered(kFontRegular, w / 2, msgY, textStr);
  }
}

void TrailScene::renderChapterEnd(Gfx& gfx) {
  const int w = gfx.width();
  drawHeader(gfx, L10N("Chapter Complete", "Capitolo Completato"), nullptr);

  int y = kTextTop + 40;
  gfx.drawTextCentered(kFontBold, w / 2, y, L10N("Chapter Finished!", "Capitolo Concluso!"));
  y += 40;

  const char* text = _engine.nodeText();
  if (text && text[0]) {
    gfx.drawTextWrapped(kFontRegular, kMarginX, y, text, w - 2 * kMarginX, 6);
  }

  y += 120;
  gfx.drawTextCentered(kFontSmall, w / 2, y, L10N("Press OK to continue", "Premi OK per proseguire"));
}

void TrailScene::renderGameOver(Gfx& gfx) {
  const int w = gfx.width();
  drawHeader(gfx, L10N("Game Over", "Fine del Viaggio"), nullptr);

  int y = kTextTop + 40;
  gfx.drawTextCentered(kFontBold, w / 2, y, L10N("The Journey Ends Here", "Il viaggio finisce qui"));
  y += 40;

  const char* text = _engine.nodeText();
  if (text && text[0]) {
    gfx.drawTextWrapped(kFontRegular, kMarginX, y, text, w - 2 * kMarginX, 6);
  }
}

void TrailScene::renderStoryWon(Gfx& gfx) {
  const int w = gfx.width();
  drawHeader(gfx, L10N("Victory!", "Vittoria!"), nullptr);

  int y = kTextTop + 40;
  gfx.drawTextCentered(kFontBold, w / 2, y, L10N("Journey Completed!", "Viaggio Completato!"));
  y += 40;

  const char* text = _engine.nodeText();
  if (text && text[0]) {
    gfx.drawTextWrapped(kFontRegular, kMarginX, y, text, w - 2 * kMarginX, 6);
  }
}

void TrailScene::render(Gfx& gfx) {
  _wCache = static_cast<int16_t>(gfx.width());
  _hCache = static_cast<int16_t>(gfx.height());

  switch (_engine.state()) {
    case trail::EngineState::Narrative:
      renderNarrative(gfx);
      break;
    case trail::EngineState::Minigame:
      renderMinigame(gfx);
      break;
    case trail::EngineState::ChapterEnded:
      renderChapterEnd(gfx);
      break;
    case trail::EngineState::GameOver:
      renderGameOver(gfx);
      break;
    case trail::EngineState::StoryWon:
      renderStoryWon(gfx);
      break;
    default:
      drawHeader(gfx, "Trail", nullptr);
      break;
  }
}
