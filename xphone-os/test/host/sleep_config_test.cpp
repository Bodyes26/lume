#include <cassert>
#include <cstdio>
#include <cstring>

#include <Preferences.h>
#include "SleepConfigStore.h"

int main() {
  Preferences::clear();

  // Test 1: Fresh store loads default configuration
  {
    SleepConfigStore store;
    store.load();
    const SleepConfig& cfg = store.config();
    assert(cfg.face == SleepFace::Auto);
    assert(cfg.customTitle[0] == '\0');
    assert(cfg.customQuote[0] == '\0');
    assert(cfg.customAuthor[0] == '\0');
    assert(cfg.showBattery == true);
    assert(cfg.showTemperature == true);
    assert(cfg.showNextEvent == true);
    assert(cfg.showReadingStats == true);
    assert(cfg.showSleepTime == true);
    assert(store.revision() == 0);
  }

  // Test 2: Update configuration and persist
  {
    SleepConfigStore store;
    store.load();
    const bool ok = store.update(
        static_cast<uint8_t>(SleepFace::Dashboard),
        "Scrivania di Maurizio",
        "La semplicita e la suprema sofisticazione.",
        "Leonardo da Vinci",
        true,   // showBattery
        false,  // showTemperature
        true,   // showNextEvent
        false,  // showReadingStats
        true    // showSleepTime
    );
    assert(ok);
    assert(store.revision() == 1);
    const SleepConfig& cfg = store.config();
    assert(cfg.face == SleepFace::Dashboard);
    assert(strcmp(cfg.customTitle, "Scrivania di Maurizio") == 0);
    assert(strcmp(cfg.customQuote, "La semplicita e la suprema sofisticazione.") == 0);
    assert(strcmp(cfg.customAuthor, "Leonardo da Vinci") == 0);
    assert(cfg.showBattery == true);
    assert(cfg.showTemperature == false);
    assert(cfg.showNextEvent == true);
    assert(cfg.showReadingStats == false);
    assert(cfg.showSleepTime == true);
  }

  // Test 3: Reload from persisted Preferences in a new store instance
  {
    SleepConfigStore store2;
    store2.load();
    const SleepConfig& cfg2 = store2.config();
    assert(cfg2.face == SleepFace::Dashboard);
    assert(strcmp(cfg2.customTitle, "Scrivania di Maurizio") == 0);
    assert(strcmp(cfg2.customQuote, "La semplicita e la suprema sofisticazione.") == 0);
    assert(strcmp(cfg2.customAuthor, "Leonardo da Vinci") == 0);
    assert(cfg2.showBattery == true);
    assert(cfg2.showTemperature == false);
    assert(cfg2.showNextEvent == true);
    assert(cfg2.showReadingStats == false);
    assert(cfg2.showSleepTime == true);
  }

  // Test 4: Clamping invalid face index to Auto
  {
    SleepConfigStore store3;
    store3.load();
    store3.update(99, "", "", "", true, true, true, true, true);
    assert(store3.config().face == SleepFace::Auto);
  }

  // Test 5: Safe string boundary clipping
  {
    SleepConfigStore store4;
    char longTitle[100];
    memset(longTitle, 'A', sizeof(longTitle) - 1);
    longTitle[sizeof(longTitle) - 1] = '\0';
    store4.update(static_cast<uint8_t>(SleepFace::Reader), longTitle, "Short quote", "Author", true, true, true, true, true);
    assert(strlen(store4.config().customTitle) <= SleepConfig::MAX_TITLE_CHARS);
  }

  printf("sleep_config: all assertions passed\n");
  return 0;
}
