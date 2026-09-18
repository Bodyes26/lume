#pragma once

// Lume — Games: the launcher tile that picks a pastime, then a mode.
//
// Two internal views (game list, then Daily/Easy/Medium/Hard for the chosen
// game) inside ONE Scene object, the same shape as SettingsScene: the
// SceneManager has no stack, so a mode picker living in its own scene would
// have nothing to restore itself from on BACK.
//
// This scene owns no puzzle state whatsoever — it only reads GAME_STATS to
// label the rows and then hands (tier, daily) to showSudoku/showNonogram/
// showMines. That keeps the whole day/streak decision in one place: the game
// scene that will actually persist the board.

#include "../Scene.h"
#include "../games/GameTypes.h"

class GamesScene : public Scene {
 public:
  void onEnter() override;
  void handleInput(Input& in) override;
  void render(Gfx& gfx) override;
  const char* const* softKeys() const override;

 private:
  enum class View : uint8_t { List, Mode };

  // Mode rows: the daily board first, then the three free-play tiers in Tier
  // order (so row index - 1 IS the Tier value — no lookup table).
  static constexpr int kModeCount = 1 + games::kTierCount;

  // Lowest selectable mode row: 1 when the clock was never set, because the
  // daily board cannot exist without a day to attribute it to.
  int firstModeRow() const { return _day == 0 ? 1 : 0; }

  void enterMode(games::Game g);
  void moveSel(int delta);
  void launchSelected();

  // Dirty rects for a selection move: only the two rows that changed ink.
  // Both return an empty rect before the first render (no cached width yet),
  // which markDirty() promotes to a full-panel repaint — correct, and it can
  // only happen on the frame that was going to be full anyway.
  XpRect listRowRect(int index) const;
  XpRect modeRowRect(int index) const;

  void renderList(Gfx& gfx);
  void renderMode(Gfx& gfx);
  void drawHeader(Gfx& gfx, const char* title, const char* right);

  View _view = View::List;
  // Deliberately NOT reset by onEnter: coming back from a game leaves the
  // cursor on the game you just played, which is where you want it.
  int _listSel = 0;
  int _modeSel = 0;
  games::Game _game = games::Game::Sudoku;
  // Civil day serial, or 0 when no source has set the clock since boot —
  // 0 disables the daily row everywhere in this scene.
  int32_t _day = 0;
  int16_t _wCache = 0;  // panel width, captured at render for the rects above
};
