#pragma once

// Lume Trail — narrative & minigame scene.
//
// Renders narrative scenes with resource status bars, story text, choices,
// and embedded micro-VM minigames. Saves to NVS automatically on choices
// and scene exit.
//
// See docs/lume/15-trail-game-design.md §10 for layout specifications.

#include <cstdint>

#include "../Scene.h"
#include "../games/trail/TrailEngine.h"
#include "../games/trail/TrailSave.h"

class TrailScene : public Scene {
 public:
  // Open with a story binary in RAM / SPIFFS
  bool open(const uint8_t* storyData, uint32_t storySize, bool resumeSaved = true);

  void onExit() override;
  void handleInput(Input& in) override;
  void render(Gfx& gfx) override;
  const char* const* softKeys() const override;

  // Sleep hook
  void persistSave();

  trail::TrailEngine& engine() { return _engine; }
  const trail::TrailEngine& engine() const { return _engine; }

 private:
  static constexpr int kMarginX = 20;
  static constexpr int kHeaderH = 46;
  static constexpr int kResBarH = 28;

  void moveChoiceSel(int delta);
  void renderNarrative(Gfx& gfx);
  void renderMinigame(Gfx& gfx);
  void renderChapterEnd(Gfx& gfx);
  void renderGameOver(Gfx& gfx);
  void renderStoryWon(Gfx& gfx);

  void drawHeader(Gfx& gfx, const char* title, const char* subtitle);
  void drawResourceBand(Gfx& gfx, int y);

  trail::TrailEngine _engine;
  int  _choiceSel = 0;
  bool _storyLoaded = false;
  int16_t _wCache = 0;
  int16_t _hCache = 0;
};
