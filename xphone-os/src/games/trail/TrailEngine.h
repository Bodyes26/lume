#pragma once

// Lume Trail — Core narrative and game engine.
//
// Pure-logic state machine: loads story chapters, navigates nodes, evaluates
// conditions, applies effects, handles choices and branches, and drives the
// micro-VM for minigames. Zero heap in loop, fully testable on host.
//
// See docs/lume/15-trail-game-design.md §4, §5, §6.

#include <cstdint>
#include <cstring>

#include "TrailFormat.h"
#include "TrailTypes.h"
#include "TrailVM.h"

namespace trail {

enum class EngineState : uint8_t {
  Unloaded     = 0,
  Narrative    = 1, // at a narrative/choice node
  Minigame     = 2, // playing a minigame
  ChapterEnded = 3, // chapter completed, waiting to advance
  StoryWon     = 4, // final chapter reached and won
  GameOver     = 5, // death or failure
};

class TrailEngine {
 public:
  TrailEngine();

  // Load a story file image. Byte buffer must remain valid while engine runs.
  bool loadStory(const uint8_t* storyData, uint32_t storySize);

  // Start a new game at chapter 0 or resume from a TrailSave snapshot.
  bool startNewGame();
  bool restoreGame(const TrailSave& save);

  // Create a snapshot for NVS persistence.
  void createSave(TrailSave& out) const;

  // ── Chapter & Node Navigation ─────────────────────────────────────────────
  bool loadChapter(int chapterIndex);
  bool advanceNode(uint16_t targetNodeId);

  // Choice interaction: selects one of the active node's available choices (0..choiceCount-1)
  // Returns false if choice is invalid or its conditions are not met.
  bool makeChoice(int choiceIndex, uint32_t randomRoll = 0);

  // Auto-advance for Event, Check, ChapterEnd nodes
  bool advanceAuto(uint32_t randomRoll = 0);

  // ── State Inspection ──────────────────────────────────────────────────────
  EngineState state() const { return _state; }
  const StoryMetaBlock* meta() const { return _reader.meta(); }
  const StoryFileHeader* header() const { return _reader.header(); }

  int currentChapter() const { return _save.chapter; }
  int currentNodeId() const { return _save.nodeId; }
  int day() const { return _save.day; }
  int distance() const { return _save.distance; }

  // Current node data
  const Node& currentNode() const { return _currentNode; }
  const char* nodeTitle() const;
  const char* nodeText() const;
  const char* choiceLabel(int index) const;
  bool isChoiceAvailable(int index) const;

  // Resources
  int16_t resource(int id) const;
  void setResource(int id, int16_t val);
  bool isResourceCritical(int id) const;

  // Companions
  bool companionPresent(int id) const { return _save.companionPresent(id); }
  int companionHealth(int id) const { return _save.companionHealth(id); }

  // Flags
  bool flag(int id) const { return _save.flagGet(id); }
  void setFlag(int id, bool val) { _save.flagSet(id, val); }

  // ── Minigame Integration ──────────────────────────────────────────────────
  TrailVM& vm() { return _vm; }
  const TrailVM& vm() const { return _vm; }
  bool startMinigame(int minigameIndex, uint32_t rngSeed = 0);
  void updateMinigameKey(Key key);
  void finishMinigame();

  // Low-level string pool access
  const char* stringAt(uint16_t offset) const;

 private:
  // Node parsing from current chapter block
  bool parseNode(uint16_t nodeId, Node& out);
  bool parseMinigame(int minigameIdx);

  // Condition and effect processing
  bool evaluateCondition(const Condition& c) const;
  bool evaluateConditions(const Condition* conds, uint8_t count) const;
  void applyEffect(const Effect& e);
  void applyEffects(const Effect* effs, uint8_t count);

  StoryReader     _reader;
  TrailSave       _save;
  EngineState     _state = EngineState::Unloaded;

  // Current chapter buffers
  const uint8_t*  _chapterData = nullptr;
  uint32_t        _chapterBytes = 0;
  ChapterHeader   _chapterHeader = {};

  Node            _currentNode = {};
  TrailVM         _vm;

  // Minigame string table pointers
  const char*     _minigameStrings[16] = {};
  uint8_t         _minigameStringCount = 0;
};

}  // namespace trail
