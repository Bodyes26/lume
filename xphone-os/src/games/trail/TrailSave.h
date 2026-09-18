#pragma once

// Lume Trail — NVS save manager.
//
// Manages 128-byte TrailSave records in NVS namespace "xphone".
// Saves are indexed by storyHash (derived from the story id).
// Checksum validated on load.
//
// See docs/lume/15-trail-game-design.md §6.

#include <cstdint>
#include <cstring>

#include "TrailTypes.h"

namespace trail {

class TrailSaveManager {
 public:
  // Load a save for `storyHash`. Returns true if a valid save was found and CRC matches.
  bool load(uint32_t storyHash, TrailSave& out);

  // Save the record to NVS. Computes checksum before writing.
  bool save(const TrailSave& record);

  // Erase save for `storyHash`.
  bool clear(uint32_t storyHash);

 private:
  static void makeKey(uint32_t storyHash, char keyOut[16]);
};

extern TrailSaveManager TRAIL_SAVES;

}  // namespace trail
