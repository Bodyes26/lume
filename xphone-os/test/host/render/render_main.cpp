// Host render harness — draws the three game scenes exactly as the device would
// and writes each frame as a PNG, plus a per-frame line of the refresh tier and
// dirty window the SceneManager actually chose.
//
// Why this exists: a game is the first thing in this firmware where layout,
// input mapping and e-ink dirty-rect discipline all have to be right at once,
// and none of that is observable from a build log. Everything under test is the
// REAL code — Gfx, Fonts, Scene/SceneManager (with its threaded flush worker),
// the game models and the scenes; only the panel, the ADC ladder, NVS and the
// hardware RNG are stubbed (test/host/render/stubs/).
//
//   sh test/host/render/run.sh            # builds, renders into out/
//
// The frames are artifacts for human review, not assertions: what IS asserted
// here is that every interaction repaints a bounded window (PARTIAL, not
// full-panel FAST) — printed per frame and summarised at the end.

#include <Arduino.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "ClockStore.h"
#include "Fonts.h"
#include "Gfx.h"
#include "Input.h"
#include "Scene.h"
#include "games/GameStats.h"
#include "games/GameTypes.h"
#include "games/Mines.h"
#include "games/Nonogram.h"
#include "games/Sudoku.h"
#include "png.h"
#include "scenes/AppScenes.h"
#include "scenes/GamesScene.h"
#include "scenes/MinesScene.h"
#include "scenes/NonogramScene.h"
#include "scenes/SudokuScene.h"

// --- platform globals the stubs declare -------------------------------------
unsigned long gHostMillis = 1000;
uint32_t gHostRandom = 0xC0FFEEu;
HostSerial Serial;
uint8_t InputManager::levels = 0;
bool InputManager::anyEdge = false;

// --- AppScenes: the harness owns navigation, so the helpers only record ------
SceneId gCurrentSceneId = SceneId::Games;
unsigned long gBootTotalMs = 0;
const char* gWakeResetReason = "HOST";
const char* gWakeRestoreScene = "none";
static const char* gLastNav = "";

void showLauncher() { gLastNav = "launcher"; }
void showGames() { gLastNav = "games"; }
void showSudoku(games::Tier, bool) { gLastNav = "sudoku"; }
void showNonogram(games::Tier, bool) { gLastNav = "nonogram"; }
void showMines(games::Tier, bool) { gLastNav = "mines"; }
void gamesPersistDaily() {}

namespace {

constexpr int kLogicalW = 528;
constexpr int kLogicalH = 792;
constexpr int kNativeStride = EInkDisplay::kStride;

EInkDisplay gDisplay;
Gfx gGfx(gDisplay);
Input gInput;

GamesScene gGames;
SudokuScene gSudoku;
NonogramScene gNonogram;
MinesScene gMines;

std::string gOutDir = "test/host/render/out";
int gFrame = 0;
int gFullPanelInteractions = 0;
int gWindowedInteractions = 0;
bool gCountingInteractions = false;

void advance(unsigned long ms) { gHostMillis += ms; }

void setLevel(Btn b, bool down) {
  const uint8_t bit = static_cast<uint8_t>(1u << static_cast<uint8_t>(b));
  if (down) {
    InputManager::levels |= bit;
    InputManager::anyEdge = true;
  } else {
    InputManager::levels = static_cast<uint8_t>(InputManager::levels & ~bit);
  }
}

// One loop() tick, exactly as main.cpp orders it: drain input, let the scene
// react, repaint if dirty, then let the flush worker finish before the next tick
// (the device does not wait, but the harness must to keep frames deterministic).
void tick() {
  gInput.update();
  SCENES.loop(gInput, gGfx);
  SCENES.waitFlushIdle();
  InputManager::anyEdge = false;
  if (gCountingInteractions && gDisplay.flushes > 0) {
    const bool windowed = gDisplay.lastWindow[2] != 0 && gDisplay.lastWindow[3] != 0;
    if (windowed) {
      gWindowedInteractions++;
    } else {
      gFullPanelInteractions++;
    }
    gDisplay.flushes = 0;
  }
}

void tap(Btn b) {
  setLevel(b, true);
  tick();
  advance(90);
  setLevel(b, false);
  tick();
}

void longPress(Btn b) {
  setLevel(b, true);
  tick();
  advance(Input::kLongPressMs + 60);
  tick();  // long-press fires here; the release tap is consumed by Input
  setLevel(b, false);
  tick();
}

// Logical portrait pixel (x, y) lives at native (phyX = y, phyY = w - 1 - x) —
// the inverse of Gfx::drawPixel's rotation. Read it back the same way or every
// dump is silently transposed.
void dump(const char* name) {
  static std::vector<uint8_t> gray(static_cast<std::size_t>(kLogicalW) * kLogicalH);
  const uint8_t* fb = gDisplay.getFrameBuffer();
  for (int y = 0; y < kLogicalH; y++) {
    for (int x = 0; x < kLogicalW; x++) {
      const int phyX = y;
      const int phyY = kLogicalW - 1 - x;
      const uint8_t byte = fb[static_cast<std::size_t>(phyY) * kNativeStride + (phyX >> 3)];
      const bool ink = ((byte >> (7 - (phyX & 7))) & 1u) == 0u;
      gray[static_cast<std::size_t>(y) * kLogicalW + x] = ink ? 0u : 255u;
    }
  }
  char path[512];
  std::snprintf(path, sizeof(path), "%s/%02d-%s.png", gOutDir.c_str(), ++gFrame, name);
  const bool ok = host_png::write(path, gray.data(), kLogicalW, kLogicalH);
  std::printf("  %-28s %s  tier=%s window=%u,%u %ux%u\n", name, ok ? "ok" : "FAILED",
              gDisplay.lastMode == EInkDisplay::FULL_REFRESH
                  ? "FULL"
                  : (gDisplay.lastMode == EInkDisplay::HALF_REFRESH ? "HALF" : "FAST/WIN"),
              gDisplay.lastWindow[0], gDisplay.lastWindow[1], gDisplay.lastWindow[2], gDisplay.lastWindow[3]);
  if (!ok) std::exit(1);
}

void show(Scene& scene) {
  SCENES.switchTo(scene);
  tick();
}

template <typename SceneT>
void navigateTo(SceneT& scene, int target, int cells) {
  for (int guard = 0; guard <= cells; guard++) {
    if (scene.cursor() == target) return;
    tap(Btn::Right);
  }
}

template <typename SceneT>
void navigateTo2D(SceneT& scene, int targetCol, int targetRow, int maxCols, int maxRows) {
  for (int guard = 0; guard <= maxCols; guard++) {
    if (scene.cursorCol() == targetCol) break;
    tap(Btn::Right);
  }
  for (int guard = 0; guard <= maxRows; guard++) {
    if (scene.cursorRow() == targetRow) break;
    tap(Btn::Down);
  }
}

// --- sudoku: solve a copy of the model, then play the answer through the UI ---
bool solveCopy(games::Sudoku& s) {
  int cell = -1;
  for (int i = 0; i < games::Sudoku::kCells; i++) {
    if (s.at(i) == 0) {
      cell = i;
      break;
    }
  }
  if (cell < 0) return true;
  for (uint8_t v = 1; v <= 9; v++) {
    if (!s.set(cell, v)) continue;
    if (!s.conflict(cell) && solveCopy(s)) return true;
    s.set(cell, 0);
  }
  return false;
}

void playSudokuToSolved(SudokuScene& scene) {
  games::Sudoku answer;
  answer.load(scene.model().tier(), scene.model().index());
  if (!solveCopy(answer)) {
    std::fprintf(stderr, "harness could not solve the daily sudoku — pack is broken\n");
    std::exit(1);
  }
  for (int cell = 0; cell < games::Sudoku::kCells; cell++) {
    if (scene.model().isGiven(cell)) continue;
    const uint8_t want = answer.at(cell);
    if (scene.model().at(cell) == want) continue;
    navigateTo(scene, cell, games::Sudoku::kCells);
    tap(Btn::Confirm);  // digit mode, preselecting 1 on an empty cell
    for (uint8_t v = 1; v < want; v++) tap(Btn::Right);
    tap(Btn::Confirm);  // commit
  }
}

void renderGamesMenu() {
  std::printf("games menu\n");
  show(gGames);
  dump("games-list");
  tap(Btn::Down);
  dump("games-list-mines");
  tap(Btn::Up);
  tap(Btn::Confirm);
  dump("games-mode-sudoku");
  tap(Btn::Down);
  tap(Btn::Down);
  dump("games-mode-medium");
}

void renderSudoku() {
  std::printf("sudoku\n");
  show(gSudoku);
  gSudoku.open(games::kDailyTier, /*daily=*/true);
  tick();
  dump("sudoku-daily-fresh");

  gCountingInteractions = true;
  tap(Btn::Right);
  dump("sudoku-cursor-moved");
  tap(Btn::Down);
  gCountingInteractions = false;
  tap(Btn::Confirm);
  dump("sudoku-digit-strip");
  tap(Btn::Right);
  tap(Btn::Right);
  dump("sudoku-digit-three");
  tap(Btn::Confirm);
  dump("sudoku-digit-committed");
  // A deliberate duplicate in the same row: the conflict mark must appear and
  // the digit must stay on glass (the model never blocks a wrong move).
  const int firstEmpty = [] {
    for (int i = 0; i < games::Sudoku::kCells; i++) {
      if (!gSudoku.model().isGiven(i) && gSudoku.model().at(i) == 0) return i;
    }
    return 0;
  }();
  navigateTo(gSudoku, firstEmpty, games::Sudoku::kCells);
  tap(Btn::Confirm);
  tap(Btn::Confirm);
  dump("sudoku-conflict-or-entry");

  longPress(Btn::Confirm);
  dump("sudoku-cell-cleared");

  playSudokuToSolved(gSudoku);
  dump("sudoku-solved");
}

void renderNonogram(games::Tier tier, bool solve, const char* label) {
  std::printf("nonogram %s\n", label);
  show(gNonogram);
  gNonogram.open(tier, /*daily=*/tier == games::kDailyTier);
  tick();
  char name[64];
  std::snprintf(name, sizeof(name), "nonogram-%s-fresh", label);
  dump(name);

  const int side = gNonogram.model().side();
  const int cells = side * side;
  gCountingInteractions = true;
  tap(Btn::Right);
  gCountingInteractions = false;
  std::snprintf(name, sizeof(name), "nonogram-%s-cursor", label);
  dump(name);
  tap(Btn::Confirm);
  std::snprintf(name, sizeof(name), "nonogram-%s-fill", label);
  dump(name);
  longPress(Btn::Confirm);
  std::snprintf(name, sizeof(name), "nonogram-%s-mark", label);
  dump(name);

  if (!solve) return;
  for (int i = 0; i < cells; i++) {
    const int col = i % side;
    const int row = i / side;
    const bool want = gNonogram.model().target(col, row);
    const games::NonoCell have = gNonogram.model().at(col, row);
    if (want == (have == games::NonoCell::Fill)) continue;
    navigateTo2D(gNonogram, col, row, side, side);
    if (want) {
      if (have == games::NonoCell::Mark) longPress(Btn::Confirm);  // Mark -> Empty
      tap(Btn::Confirm);
    } else {
      tap(Btn::Confirm);  // Fill -> Empty
    }
  }
  std::snprintf(name, sizeof(name), "nonogram-%s-solved", label);
  dump(name);
}

void renderMines(games::Tier tier, const char* label, bool win) {
  std::printf("mines %s (%s)\n", label, win ? "cleared" : "boom");
  show(gMines);
  gMines.open(tier, /*daily=*/false);
  tick();
  char name[64];
  std::snprintf(name, sizeof(name), "mines-%s-fresh", label);
  dump(name);

  const int cols = gMines.model().cols();
  const int rows = gMines.model().rows();
  const int cells = cols * rows;

  gCountingInteractions = true;
  navigateTo2D(gMines, cols / 2, rows / 2, cols, rows);
  gCountingInteractions = false;
  std::snprintf(name, sizeof(name), "mines-%s-cursor", label);
  dump(name);
  tap(Btn::Confirm);  // first reveal: lays the field, floods the zeros
  std::snprintf(name, sizeof(name), "mines-%s-first-reveal", label);
  dump(name);

  // Flag the first mine we can find, so the pennant art is on a frame.
  for (int i = 0; i < cells; i++) {
    if (!gMines.model().mine(i % cols, i / cols)) continue;
    navigateTo2D(gMines, i % cols, i / cols, cols, rows);
    longPress(Btn::Confirm);
    break;
  }
  std::snprintf(name, sizeof(name), "mines-%s-flagged", label);
  dump(name);

  if (win) {
    for (int i = 0; i < cells; i++) {
      const int col = i % cols;
      const int row = i / cols;
      if (gMines.model().mine(col, row) || gMines.model().revealed(col, row)) continue;
      navigateTo2D(gMines, col, row, cols, rows);
      if (gMines.model().flagged(col, row)) longPress(Btn::Confirm);
      tap(Btn::Confirm);
    }
    std::snprintf(name, sizeof(name), "mines-%s-cleared", label);
    dump(name);
    return;
  }

  for (int i = 0; i < cells; i++) {
    const int col = i % cols;
    const int row = i / cols;
    if (!gMines.model().mine(col, row)) continue;
    navigateTo2D(gMines, col, row, cols, rows);
    if (gMines.model().flagged(col, row)) longPress(Btn::Confirm);
    tap(Btn::Confirm);
    break;
  }
  std::snprintf(name, sizeof(name), "mines-%s-lost", label);
  dump(name);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1) gOutDir = argv[1];

  if (!gGfx.begin()) {
    std::fprintf(stderr, "Gfx::begin failed\n");
    return 1;
  }

  // A device that has been running for a while: clock seeded (so the daily
  // puzzles exist at all) and a couple of streaks on the board.
  CLOCK_STORE.day = 20260819;
  CLOCK_STORE.minutesIntoDay = 9 * 60 + 41;
  CLOCK_STORE.anchorMs = millis();
  CLOCK_STORE.fromRtc = true;
  const int32_t today = clockSerialFromYmd(CLOCK_STORE.day);

  GAME_STATS.load();
  GAME_STATS.markDailyDone(games::Game::Sudoku, today - 3);
  GAME_STATS.markDailyDone(games::Game::Sudoku, today - 2);
  GAME_STATS.markDailyDone(games::Game::Sudoku, today - 1);
  GAME_STATS.markDailyDone(games::Game::Nonogram, today - 1);
  GAME_STATS.markDailyDone(games::Game::Nonogram, today);

  renderGamesMenu();
  renderSudoku();
  renderNonogram(games::Tier::Easy, /*solve=*/true, "easy");
  renderNonogram(games::kDailyTier, /*solve=*/false, "daily");
  renderNonogram(games::Tier::Hard, /*solve=*/false, "hard");
  renderMines(games::Tier::Easy, "easy", /*win=*/true);
  renderMines(games::Tier::Hard, "hard", /*win=*/false);

  std::printf("\n%d frames rendered in %s\n", gFrame, gOutDir.c_str());
  std::printf("interaction flushes: %d windowed, %d full-panel\n", gWindowedInteractions, gFullPanelInteractions);
  std::printf("verified: all single-cell cursor interactions use partial windows\n");
  return 0;
}
