#include "TrailSave.h"

#include <Preferences.h>
#include <cstdio>

namespace trail {

namespace {
constexpr const char* kPrefsNamespace = "xphone";
}  // namespace

TrailSaveManager TRAIL_SAVES;

void TrailSaveManager::makeKey(uint32_t storyHash, char keyOut[16]) {
  // NVS keys are max 15 chars. "tr_" + 8 hex digits = 11 chars.
  snprintf(keyOut, 16, "tr_%08x", static_cast<unsigned int>(storyHash));
}

bool TrailSaveManager::load(uint32_t storyHash, TrailSave& out) {
  char key[16];
  makeKey(storyHash, key);

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, /*readOnly=*/true)) return false;

  const std::size_t len = prefs.getBytesLength(key);
  if (len != sizeof(TrailSave)) {
    prefs.end();
    return false;
  }

  TrailSave temp;
  if (prefs.getBytes(key, &temp, sizeof(TrailSave)) != sizeof(TrailSave)) {
    prefs.end();
    return false;
  }
  prefs.end();

  // Validate checksum
  uint16_t expected = crc16(reinterpret_cast<const uint8_t*>(&temp), sizeof(TrailSave) - sizeof(uint16_t));
  if (temp.checksum != expected || temp.storyHash != storyHash) {
    return false;
  }

  out = temp;
  return true;
}

bool TrailSaveManager::save(const TrailSave& record) {
  TrailSave toWrite = record;
  toWrite.checksum = crc16(reinterpret_cast<const uint8_t*>(&toWrite), sizeof(TrailSave) - sizeof(uint16_t));

  char key[16];
  makeKey(toWrite.storyHash, key);

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, /*readOnly=*/false)) return false;

  bool ok = prefs.putBytes(key, &toWrite, sizeof(TrailSave)) == sizeof(TrailSave);
  prefs.end();
  return ok;
}

bool TrailSaveManager::clear(uint32_t storyHash) {
  char key[16];
  makeKey(storyHash, key);

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, /*readOnly=*/false)) return false;

  bool ok = prefs.remove(key);
  prefs.end();
  return ok;
}

}  // namespace trail
