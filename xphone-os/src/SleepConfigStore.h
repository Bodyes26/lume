#pragma once

// Lume — sleep screen configuration store.
//
// Manages customizable sleep poster preferences:
//   - Sleep face mode (Auto, Dashboard, Reader, Reminders, Quote, Minimal)
//   - Custom poster title (e.g. "Scrivania di Maurizio", "Lume X3", "Oggi")
//   - Custom quote & author for the Quote face or Reading Companion face
//   - Modular feature toggles: battery, temperature (°C from DS3231),
//     next calendar event, reading stats (pages today / streak), sleep timestamp
//
// Persisted in NVS flash ("xphone" namespace) so settings survive deep sleep
// and full power cycles. Updated over BLE via "sleep.config" JSON cards.

#include <cstddef>
#include <cstdint>

enum class SleepFace : uint8_t {
  Auto = 0,        // Smart context: Reader if active book, Workout if active, Dashboard otherwise
  Dashboard = 1,   // Daily glance: Date + Agenda + Priority Checklist + Batt/Temp
  Reader = 2,      // Reading companion: Book Title + Author + Progress Bar + Reading Stats + Quote
  Reminders = 3,   // Focus checklist: Full-page EventKit checklist + next event
  Quote = 4,       // Literary / Personal Quote & Mantra + Date + Status
  Minimal = 5,     // Pure Lume: Geometric Emblem, Date, Sleep Timestamp, Battery & Temp
};

struct SleepConfig {
  static constexpr std::size_t MAX_TITLE_CHARS = 48;
  static constexpr std::size_t MAX_QUOTE_CHARS = 160;
  static constexpr std::size_t MAX_AUTHOR_CHARS = 48;

  SleepFace face = SleepFace::Auto;
  char customTitle[MAX_TITLE_CHARS + 1] = {0};
  char customQuote[MAX_QUOTE_CHARS + 1] = {0};
  char customAuthor[MAX_AUTHOR_CHARS + 1] = {0};
  bool showBattery = true;
  bool showTemperature = true;
  bool showNextEvent = true;
  bool showReadingStats = true;
  bool showSleepTime = true;
};

class SleepConfigStore {
 public:
  SleepConfigStore();

  // Load preferences from NVS ("xphone" namespace). Safe to call at boot.
  void load();

  // Save current preferences to NVS. Returns true on success.
  bool save();

  // Update configuration from parsed card fields. Persists to NVS and bumps revision.
  bool update(uint8_t face, const char* title, const char* quote, const char* author,
              bool showBattery, bool showTemperature, bool showNextEvent,
              bool showReadingStats, bool showSleepTime);

  const SleepConfig& config() const { return _config; }
  uint32_t revision() const { return _revision; }

 private:
  SleepConfig _config;
  uint32_t _revision = 0;
  bool _loaded = false;
};

extern SleepConfigStore SLEEP_CONFIG;
