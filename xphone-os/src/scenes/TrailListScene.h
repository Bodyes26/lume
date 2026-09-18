#pragma once

// Lume Trail — Story catalog picker scene.
//
// Shows the list of available stories (bundled in flash or installed in SPIFFS),
// displays progress from NVS saves, and launches the selected story into TrailScene.
//
// See docs/lume/15-trail-game-design.md §10.3.

#include "../Scene.h"
#include "../games/trail/TrailSave.h"
#include "../games/trail/TrailTypes.h"

class TrailListScene : public Scene {
 public:
  void onEnter() override;
  void handleInput(Input& in) override;
  void render(Gfx& gfx) override;
  const char* const* softKeys() const override;

  int selectedIndex() const { return _selected; }

 private:
  static constexpr int kMarginX = 20;
  static constexpr int kHeaderH = 46;
  static constexpr int kListTop = 62;
  static constexpr int kListRowH = 75;
  static constexpr int kListBoxH = 71;
  static constexpr int kMaxStories = 8;

  struct StoryItem {
    char id[trail::kStoryIdLen];
    char title[trail::kStoryTitleLen];
    uint16_t chapterCount;
    const uint8_t* data;
    uint32_t size;
  };

  void scanStories();
  void moveSel(int delta);
  void launchSelected();

  StoryItem _stories[kMaxStories] = {};
  int _storyCount = 0;
  int _selected = 0;
  int16_t _wCache = 0;
};
