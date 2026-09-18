#include "TrailListScene.h"

#include <cstdio>
#include <cstring>

#include "../Fonts.h"
#include "../LumeLocale.h"
#include "AppScenes.h"

#include "../games/trail/DefaultStoryPack.h"
#include "../games/trail/TrailFormat.h"

void TrailListScene::onEnter() {
  scanStories();
  if (_selected >= _storyCount) _selected = _storyCount > 0 ? _storyCount - 1 : 0;
  markDirty();
}

void TrailListScene::scanStories() {
  _storyCount = 0;

  // 1. Check bundled story in flash
  if (trail_bundled::defaultStoryData && trail_bundled::defaultStorySize > 0) {
    trail::StoryReader reader(trail_bundled::defaultStoryData, trail_bundled::defaultStorySize);
    if (reader.isValid()) {
      const auto* hdr = reader.header();
      strncpy(_stories[_storyCount].id, hdr->id, trail::kStoryIdLen);
      strncpy(_stories[_storyCount].title, hdr->title, trail::kStoryTitleLen);
      _stories[_storyCount].chapterCount = hdr->chapterCount;
      _stories[_storyCount].data = trail_bundled::defaultStoryData;
      _stories[_storyCount].size = trail_bundled::defaultStorySize;
      _storyCount++;
    }
  }

  // Fallback / placeholder if no bundled story is linked yet
  if (_storyCount == 0) {
    strncpy(_stories[0].id, "silk_road", trail::kStoryIdLen);
    strncpy(_stories[0].title, L10N("The Silk Road", "La Via della Seta"), trail::kStoryTitleLen);
    _stories[0].chapterCount = 8;
    _stories[0].data = nullptr;
    _stories[0].size = 0;
    _storyCount = 1;
  }
}

const char* const* TrailListScene::softKeys() const {
  static const char* const kKeys[] = {
    L10N("BACK", "INDIETRO"),
    "OK",
    L10N("UP", "SU"),
    L10N("DOWN", "GIU")
  };
  return kKeys;
}

void TrailListScene::moveSel(int delta) {
  if (_storyCount <= 0) return;
  int next = _selected + delta;
  if (next < 0) next = _storyCount - 1;
  if (next >= _storyCount) next = 0;
  _selected = next;
  markDirty();
}

void TrailListScene::launchSelected() {
  if (_selected < 0 || _selected >= _storyCount) return;
  const auto& item = _stories[_selected];
  if (item.data && item.size > 0) {
    showTrailStory(item.data, item.size);
  }
}

void TrailListScene::handleInput(Input& in) {
  if (in.wasPressed(Btn::Back)) {
    showGames();
    return;
  }
  if (in.wasPressed(Btn::Up))   moveSel(-1);
  if (in.wasPressed(Btn::Down)) moveSel(+1);
  if (in.wasPressed(Btn::Confirm)) {
    launchSelected();
  }
}

void TrailListScene::render(Gfx& gfx) {
  const int w = gfx.width();
  _wCache = static_cast<int16_t>(w);

  // Header
  gfx.drawText(kFontBold, kMarginX, 8, L10N("Adventures", "Avventure"));
  gfx.fillRect(0, kHeaderH - 2, w, 2, true);

  for (int i = 0; i < _storyCount; ++i) {
    const auto& story = _stories[i];
    int rowY = kListTop + i * kListRowH;

    // Selection ring
    if (i == _selected) {
      gfx.drawRoundedRect(8, rowY - 6, w - 16, kListBoxH, 12, 3, true);
    }

    gfx.drawText(kFontBold, kMarginX, rowY, story.title);

    // Read progress from NVS save
    trail::TrailSave save;
    char status[64];
    uint32_t hash = trail::storyHash(story.id);

    if (trail::TRAIL_SAVES.load(hash, save)) {
      snprintf(status, sizeof(status), "%s %d/%d · %s %d",
               L10N("Cap.", "Cap."), save.chapter + 1, story.chapterCount,
               L10N("Day", "Giorno"), save.day);
    } else {
      snprintf(status, sizeof(status), "%s (%d %s)",
               L10N("New game", "Nuova partita"), story.chapterCount,
               L10N("chapters", "capitoli"));
    }

    gfx.drawText(kFontSmall, kMarginX, rowY + gfx.lineHeight(kFontBold) + 2, status);
  }

  // Hint at bottom
  int hintY = kListTop + _storyCount * kListRowH + 16;
  gfx.drawTextCentered(kFontSmall, w / 2, hintY,
                       L10N("Choose a story to begin your journey.",
                            "Scegli una storia per iniziare il viaggio."));
}
