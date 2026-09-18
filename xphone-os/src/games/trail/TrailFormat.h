#pragma once

// Lume Trail — binary format definitions and low-level parsing helpers.
//
// Defines the exact binary layout of a `.story` file and provides zero-heap,
// zero-copy reader structs for reading story metadata, chapter tables,
// nodes, minigames, and string pools from RAM or memory-mapped flash.
//
// See docs/lume/15-trail-game-design.md §4 for specification.

#include <cstdint>
#include <cstring>

#include "TrailTypes.h"

namespace trail {

// ─── Packed on-disk header (exactly 64 bytes) ────────────────────────────────

struct alignas(4) StoryFileHeader {
  char     magic[4];       // "LTRL"
  uint8_t  version;        // 1
  uint8_t  flags;          // reserved
  uint16_t chapterCount;
  char     id[24];         // null-terminated
  char     title[32];      // null-terminated
};

static_assert(sizeof(StoryFileHeader) == 64, "StoryFileHeader must be 64 bytes");

// ─── Packed metadata block (resources & companions) ─────────────────────────

struct StoryMetaBlock {
  uint8_t  resourceCount;
  uint8_t  companionCount;
  uint8_t  reserved[2];
  ResourceDef  resources[kMaxResources];
  CompanionDef companions[kMaxCompanions];
};

// ─── Packed Chapter Header ──────────────────────────────────────────────────

struct ChapterHeader {
  uint16_t nodeCount;
  uint16_t minigameCount;
  uint16_t stringCount;
  uint16_t imageCount;
  uint32_t nodesOffset;      // offset from start of chapter block
  uint32_t minigamesOffset;
  uint32_t stringsOffset;
  uint32_t imagesOffset;
  uint32_t totalBytes;
};

// ─── Packed Minigame Descriptor inside Chapter ──────────────────────────────

struct MinigameDescriptor {
  uint8_t  gridW;
  uint8_t  gridH;
  uint8_t  stringCount;
  uint8_t  reserved;
  char     cellChars[16];
  uint8_t  cellStyles[16];
  uint16_t stringOffsets[16]; // offsets in string pool for minigame strings
  uint16_t setupLen;
  uint16_t keyLens[6];        // Up, Down, Left, Right, Confirm, Back
  // Followed by bytecode: setupCode, then keyCodes[0..5]
};

// ─── Packed Image Descriptor ────────────────────────────────────────────────

struct ImageDescriptor {
  uint16_t width;
  uint16_t height;
  uint32_t rleBytes;
  // Followed by `rleBytes` of RLE-compressed 1-bit bitmap data
};

// ─── Reader Helper ──────────────────────────────────────────────────────────

class StoryReader {
 public:
  StoryReader() = default;
  StoryReader(const uint8_t* data, uint32_t size) : _data(data), _size(size) {}

  bool isValid() const {
    if (!_data || _size < sizeof(StoryFileHeader) + sizeof(StoryMetaBlock)) return false;
    const auto* h = header();
    return h->magic[0] == 'L' && h->magic[1] == 'T' && h->magic[2] == 'R' && h->magic[3] == 'L' &&
           h->version == kFormatVersion && h->chapterCount > 0;
  }

  const StoryFileHeader* header() const {
    return reinterpret_cast<const StoryFileHeader*>(_data);
  }

  const StoryMetaBlock* meta() const {
    if (_size < sizeof(StoryFileHeader) + sizeof(StoryMetaBlock)) return nullptr;
    return reinterpret_cast<const StoryMetaBlock*>(_data + sizeof(StoryFileHeader));
  }

  uint32_t chapterOffset(int chapterIdx) const {
    if (!isValid()) return 0;
    const auto* h = header();
    if (chapterIdx < 0 || chapterIdx >= h->chapterCount) return 0;
    uint32_t tableOffset = sizeof(StoryFileHeader) + sizeof(StoryMetaBlock);
    if (tableOffset + (chapterIdx + 1) * sizeof(uint32_t) > _size) return 0;
    const uint32_t* table = reinterpret_cast<const uint32_t*>(_data + tableOffset);
    return table[chapterIdx];
  }

  const uint8_t* chapterData(int chapterIdx, uint32_t* outLen = nullptr) const {
    uint32_t off = chapterOffset(chapterIdx);
    if (off == 0 || off >= _size) return nullptr;
    if (off + sizeof(ChapterHeader) > _size) return nullptr;
    const auto* ch = reinterpret_cast<const ChapterHeader*>(_data + off);
    if (outLen) *outLen = ch->totalBytes;
    return _data + off;
  }

  const uint8_t* data() const { return _data; }
  uint32_t size() const { return _size; }

 private:
  const uint8_t* _data = nullptr;
  uint32_t _size = 0;
};

}  // namespace trail
