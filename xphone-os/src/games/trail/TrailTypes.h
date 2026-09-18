#pragma once

// Lume Trail — shared types and constants for the narrative-adventure game
// engine. Everything under src/games/trail/ is PURE LOGIC: <cstdint>/<cstring>
// only (plus Preferences.h in TrailSave), no Arduino, no Gfx, no heap, no
// FreeRTOS. Host-testable, same split as the pastime games.
//
// See docs/lume/15-trail-game-design.md for the full specification.

#include <cstdint>
#include <cstring>

namespace trail {

// ─── Limits ─────────────────────────────────────────────────────────────────

constexpr int kMaxResources     = 8;
constexpr int kMaxCompanions    = 6;
constexpr int kMaxChoices       = 4;
constexpr int kMaxEffects       = 8;   // per choice or per node
constexpr int kMaxConditions    = 4;   // per choice
constexpr int kMaxFlags         = 256; // 32 bytes of bitfield
constexpr int kFlagBytes        = kMaxFlags / 8;
constexpr int kMaxChapters      = 255;
constexpr int kMaxMinigameRes   = 16;  // minigame results stored in save
constexpr int kStoryIdLen       = 24;
constexpr int kStoryTitleLen    = 32;
constexpr int kResNameLen       = 24;
constexpr int kCompNameLen      = 16;
constexpr int kCompTraitLen     = 16;

// ─── Binary format magic ────────────────────────────────────────────────────

constexpr char kStoryMagic[4] = {'L', 'T', 'R', 'L'};
constexpr uint8_t kFormatVersion = 1;

// ─── Resource display style ─────────────────────────────────────────────────

enum class ResIcon : uint8_t { Bar = 0, Number = 1, Fraction = 2 };

// ─── Node type ──────────────────────────────────────────────────────────────

enum class NodeType : uint8_t {
  Narrative = 0,   // text + choices
  Event     = 1,   // text + effects + auto-advance
  Check     = 2,   // conditional branch (no player choice)
  Minigame  = 3,   // launch a minigame
  Shop      = 4,   // buy/sell items
  ChapterEnd = 5,  // end of chapter
  GameOver  = 6,   // death/failure
};

// ─── Effect type ────────────────────────────────────────────────────────────

enum class EffectType : uint8_t {
  ResourceDelta    = 0,
  ResourceSet      = 1,
  CompanionJoin    = 2,
  CompanionLeave   = 3,
  CompanionHealth  = 4,
  FlagSet          = 5,
  FlagClear        = 6,
  Distance         = 7,
  Day              = 8,
};

// ─── Condition type ─────────────────────────────────────────────────────────

enum class CondType : uint8_t {
  ResourceMin      = 0,
  ResourceMax      = 1,
  CompanionAlive   = 2,
  CompanionDead    = 3,
  FlagSet          = 4,
  FlagClear        = 5,
};

// ─── Structs for in-memory representation (read from binary) ────────────────

struct ResourceDef {
  char     name[kResNameLen];
  ResIcon  icon;
  int16_t  max;
  int16_t  start;
  int16_t  critical;
};

struct CompanionDef {
  char    name[kCompNameLen];
  char    trait[kCompTraitLen];
};

struct StoryMeta {
  char          id[kStoryIdLen];
  char          title[kStoryTitleLen];
  uint16_t      chapterCount;
  uint8_t       resourceCount;
  uint8_t       companionCount;
  ResourceDef   resources[kMaxResources];
  CompanionDef  companions[kMaxCompanions];
};

struct Effect {
  EffectType type;
  uint8_t    id;       // resource/companion/flag index
  int16_t    value;
};

struct Condition {
  CondType  type;
  uint8_t   id;
  int16_t   value;
};

struct Choice {
  // Text is stored separately in the string pool; this is an offset/length.
  uint16_t  labelOff;
  uint16_t  labelLen;
  uint16_t  nextNode;
  uint8_t   probability;   // 0-100, 0 = certain
  uint16_t  failNode;      // if probability > 0 and dice fails
  int8_t    minigameId = -1;    // -1 = none
  uint8_t   conditionCount;
  uint8_t   effectCount;
  Condition conditions[kMaxConditions];
  Effect    effects[kMaxEffects];
};

struct Node {
  NodeType  type;
  int8_t    illustrationId = -1;   // -1 = none
  uint16_t  titleOff;
  uint16_t  titleLen;
  uint16_t  textOff;
  uint16_t  textLen;
  uint16_t  nextNode;         // for Event/Check/ChapterEnd
  uint16_t  passNode;         // Check: condition pass
  uint16_t  failNode;         // Check: condition fail
  int8_t    minigameId = -1;       // Minigame node: which minigame (-1 = none)
  uint16_t  winNode;          // Minigame: on win
  uint16_t  loseNode;         // Minigame: on lose
  uint8_t   choiceCount;
  uint8_t   effectCount;      // Event effects
  uint8_t   conditionCount;   // Check conditions
  Choice    choices[kMaxChoices];
  Effect    effects[kMaxEffects];
  Condition conditions[kMaxConditions];
};

// ─── Save format (NVS) ─────────────────────────────────────────────────────

constexpr int kSaveBytes = 128;

struct alignas(4) TrailSave {
  uint32_t  storyHash;                      // hash of story id (verification)
  uint16_t  chapter;                        // current chapter
  uint16_t  nodeId;                         // current node in chapter
  uint16_t  day;                            // day of travel
  int16_t   distance;                       // distance traveled
  int16_t   resources[kMaxResources];       // current resource values
  uint8_t   companionState[kMaxCompanions]; // bit 0 = present, bits 1-7 = health/2
  uint8_t   flags[kFlagBytes];              // 256 decision flags
  uint8_t   chapterDone[kMaxChapters / 8 + 1]; // completed chapters bitmask
  uint8_t   minigameResults[kMaxMinigameRes];   // outcomes for future branch
  uint16_t  checksum;                       // CRC-16 of preceding bytes

  // Helpers
  bool flagGet(int i) const {
    if (i < 0 || i >= kMaxFlags) return false;
    return (flags[i >> 3] >> (i & 7)) & 1u;
  }
  void flagSet(int i, bool v) {
    if (i < 0 || i >= kMaxFlags) return;
    if (v) flags[i >> 3] |= (1u << (i & 7));
    else   flags[i >> 3] &= ~(1u << (i & 7));
  }
  bool companionPresent(int i) const {
    if (i < 0 || i >= kMaxCompanions) return false;
    return companionState[i] & 1u;
  }
  int companionHealth(int i) const {
    if (i < 0 || i >= kMaxCompanions) return 0;
    return (companionState[i] >> 1) * 2;  // stored as health/2
  }
  void setCompanion(int i, bool present, int health) {
    if (i < 0 || i >= kMaxCompanions) return;
    uint8_t h = static_cast<uint8_t>((health < 0 ? 0 : health > 200 ? 100 : health / 2));
    companionState[i] = (present ? 1u : 0u) | (h << 1);
  }
  void clear() { memset(this, 0, sizeof(*this)); }
};

static_assert(sizeof(TrailSave) <= kSaveBytes, "TrailSave exceeds NVS budget");

// ─── Utility ────────────────────────────────────────────────────────────────

// djb2 hash of a null-terminated string — stable across builds, used to tag
// saves to their story id.
inline uint32_t storyHash(const char* id) {
  uint32_t h = 5381;
  while (*id) h = h * 33 + static_cast<uint8_t>(*id++);
  return h;
}

// CRC-16/CCITT for save checksums.
inline uint16_t crc16(const uint8_t* data, int len) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; ++b)
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
  }
  return crc;
}

}  // namespace trail
