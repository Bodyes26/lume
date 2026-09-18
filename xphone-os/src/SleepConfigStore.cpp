#include "SleepConfigStore.h"

#include <Preferences.h>
#include <cstring>

#include "ble/Utf8Clip.h"

namespace {
constexpr const char* kPrefsNamespace = "xphone";
constexpr const char* kPrefsFaceKey = "slpFace";
constexpr const char* kPrefsTitleKey = "slpTitle";
constexpr const char* kPrefsQuoteKey = "slpQuote";
constexpr const char* kPrefsAuthorKey = "slpAuthor";
constexpr const char* kPrefsFlagsKey = "slpFlags";

// Flag bit positions
constexpr uint8_t kFlagBattery = 1 << 0;
constexpr uint8_t kFlagTemperature = 1 << 1;
constexpr uint8_t kFlagNextEvent = 1 << 2;
constexpr uint8_t kFlagReadingStats = 1 << 3;
constexpr uint8_t kFlagSleepTime = 1 << 4;
}  // namespace

SleepConfigStore SLEEP_CONFIG;

SleepConfigStore::SleepConfigStore() = default;

void SleepConfigStore::load() {
  if (_loaded) return;
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, /*readOnly=*/true)) {
    _loaded = true;
    return;
  }

  if (prefs.isKey(kPrefsFaceKey)) {
    const uint8_t rawFace = prefs.getUChar(kPrefsFaceKey, 0);
    _config.face = (rawFace <= static_cast<uint8_t>(SleepFace::Minimal))
                       ? static_cast<SleepFace>(rawFace)
                       : SleepFace::Auto;
  }

  if (prefs.isKey(kPrefsTitleKey)) {
    char buf[SleepConfig::MAX_TITLE_CHARS + 1] = {0};
    prefs.getString(kPrefsTitleKey, buf, sizeof(buf));
    clipUtf8(_config.customTitle, sizeof(_config.customTitle), buf);
  }

  if (prefs.isKey(kPrefsQuoteKey)) {
    char buf[SleepConfig::MAX_QUOTE_CHARS + 1] = {0};
    prefs.getString(kPrefsQuoteKey, buf, sizeof(buf));
    clipUtf8(_config.customQuote, sizeof(_config.customQuote), buf);
  }

  if (prefs.isKey(kPrefsAuthorKey)) {
    char buf[SleepConfig::MAX_AUTHOR_CHARS + 1] = {0};
    prefs.getString(kPrefsAuthorKey, buf, sizeof(buf));
    clipUtf8(_config.customAuthor, sizeof(_config.customAuthor), buf);
  }

  if (prefs.isKey(kPrefsFlagsKey)) {
    const uint8_t flags = prefs.getUChar(kPrefsFlagsKey, 0x1F);
    _config.showBattery = (flags & kFlagBattery) != 0;
    _config.showTemperature = (flags & kFlagTemperature) != 0;
    _config.showNextEvent = (flags & kFlagNextEvent) != 0;
    _config.showReadingStats = (flags & kFlagReadingStats) != 0;
    _config.showSleepTime = (flags & kFlagSleepTime) != 0;
  }

  prefs.end();
  _loaded = true;
}

bool SleepConfigStore::save() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, /*readOnly=*/false)) return false;

  prefs.putUChar(kPrefsFaceKey, static_cast<uint8_t>(_config.face));

  if (_config.customTitle[0] != '\0') {
    prefs.putString(kPrefsTitleKey, _config.customTitle);
  } else {
    prefs.remove(kPrefsTitleKey);
  }

  if (_config.customQuote[0] != '\0') {
    prefs.putString(kPrefsQuoteKey, _config.customQuote);
  } else {
    prefs.remove(kPrefsQuoteKey);
  }

  if (_config.customAuthor[0] != '\0') {
    prefs.putString(kPrefsAuthorKey, _config.customAuthor);
  } else {
    prefs.remove(kPrefsAuthorKey);
  }

  uint8_t flags = 0;
  if (_config.showBattery) flags |= kFlagBattery;
  if (_config.showTemperature) flags |= kFlagTemperature;
  if (_config.showNextEvent) flags |= kFlagNextEvent;
  if (_config.showReadingStats) flags |= kFlagReadingStats;
  if (_config.showSleepTime) flags |= kFlagSleepTime;
  prefs.putUChar(kPrefsFlagsKey, flags);

  prefs.end();
  return true;
}

bool SleepConfigStore::update(uint8_t face, const char* title, const char* quote, const char* author,
                              bool showBattery, bool showTemperature, bool showNextEvent,
                              bool showReadingStats, bool showSleepTime) {
  _config.face = (face <= static_cast<uint8_t>(SleepFace::Minimal))
                     ? static_cast<SleepFace>(face)
                     : SleepFace::Auto;

  clipUtf8(_config.customTitle, sizeof(_config.customTitle), title);
  clipUtf8(_config.customQuote, sizeof(_config.customQuote), quote);
  clipUtf8(_config.customAuthor, sizeof(_config.customAuthor), author);

  _config.showBattery = showBattery;
  _config.showTemperature = showTemperature;
  _config.showNextEvent = showNextEvent;
  _config.showReadingStats = showReadingStats;
  _config.showSleepTime = showSleepTime;

  ++_revision;
  _loaded = true;
  return save();
}
