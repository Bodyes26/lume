#include "Sleep.h"

#include <Arduino.h>

#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <Preferences.h>
#include <Wire.h>

#include <cstdio>
#include <cstring>

#include "esp_system.h"

#include "driver/gpio.h"
#include "esp_sleep.h"

#include "BlockStatusStore.h"
#include "ClockStore.h"
#include "Fonts.h"
#include "NotificationStore.h"
#include "Gfx.h"
#include "Input.h"
#include "LumeLocale.h"
#include "RemindersStore.h"
#include "Scene.h"
#include "TodayStore.h"
#include "ble/CompanionBleService.h"
#include "scenes/AppScenes.h"
#include "scenes/RemindersScene.h"
#include "scenes/WorkoutScene.h"
#include "WorkoutStore.h"
#include <ArduinoJson.h>
#include "BatteryGauge.h"
#include "Ds3231.h"
#include "SleepConfigStore.h"
#include "art/LumeMark.h"
#include "reader/ReadingStats.h"

namespace {

// M4.2 last-scene restore — NVS flash (Arduino Preferences over the ESP-IDF nvs
// partition). RTC memory (both .rtc.bss and RTC_NOINIT) proved unreliable on
// this hardware: the X3's power button triggers a full power-on reset that
// clears the entire RTC domain, so nothing written before deep sleep survived
// the wake. NVS survives ANY reset including full power loss (the same store
// BLE bonds already use), so the scene id persists across the power-button
// power cycle. The key is consume-once: boot() reads then removes it, so a cold
// boot with no prior sleep finds no key and lands on the launcher.
constexpr const char* kPrefsNamespace = "xphone";
constexpr const char* kPrefsSceneKey = "lastScene";

// M4.3 Block snapshot keys (same "xphone" namespace). Written fresh every sleep
// when a block is active, cleared when it is not — so a finished block never
// resurrects on wake. Names are <=15 chars (NVS key limit).
constexpr const char* kBlkActiveKey = "blkActive";
constexpr const char* kBlkBreakKey = "blkBreak";
constexpr const char* kBlkRemainKey = "blkRemain";
constexpr const char* kBlkDurKey = "blkDur";
constexpr const char* kBlkPresetKey = "blkPreset";
constexpr const char* kBlkEndsKey = "blkEnds";
// Completion counters — persisted every sleep REGARDLESS of active state (a
// finished block still contributes today's count / streak / total).
constexpr const char* kBlkTodayKey = "blkToday";
constexpr const char* kBlkStreakKey = "blkStreak";
constexpr const char* kBlkTotalKey = "blkTotal";
// Last Today / Priorities card JSON — re-seeded on wake so those scenes show
// cached data (with a "syncing" indicator) instead of a blank screen.
constexpr const char* kTodayCardKey = "todayCard";
constexpr const char* kRemCardKey = "remCard";
constexpr const char* kWorkoutCardKey = "wkCard";
// Newest notifications as a raw Entry blob (NotificationStore::snapshot), so
// the Notifications app shows the last-known list instantly on wake while the
// ANCS backfill refreshes it (the store was RAM-only — every wake was blank).
constexpr const char* kNotifStoreKey = "notifStore";
constexpr const char* kNotifTombKey = "notifTombs";
constexpr std::size_t kNotifPersistMax = 10;  // about 2.2 KB with the current Entry layout

// ONE shared persistence scratch pair for both the sleep-time snapshot and
// the boot-time restore. The two paths can never run concurrently (both are
// main-loop only: one entering deep sleep, the other inside boot()), so the
// four per-site static blobs this replaces were pure BSS duplication
// (~2.7 KB). Static rather than stack because sleepNow() runs deep in the
// loop task's stack.
uint8_t gNotifScratch[kNotifPersistMax * sizeof(NotificationStore::Entry)];
uint8_t gTombScratch[NotificationStore::TOMBSTONE_CAPACITY * sizeof(NotificationStore::Tombstone)];

// ---------------------------------------------------------------------------
// Sleep screen. FULL refresh — the panel then holds this image at ~0 current
// (e-ink retains unpowered; UC8253 DEEP_SLEEP 0x07, SSD1677 DEEP_SLEEP 0x10).
// M4.1 dormant frame: when the priorities store holds a snapshot, the glass
// holds the day's list all night
// (PrioritiesScene::renderDormant — list + moon/"lume" stamp + wake hint);
// otherwise the original centered "lume" wordmark over a short rule, wake
// hint at the bottom. The panel controller stays powered while awake; there is
// no idle-sleep middle state.
// v0.4: the DS3231 gives real wall time, but this frame is painted once and
// then frozen for hours — and no timed wake can repaint it (all six RTC-capable
// GPIOs are taken and the battery latch cuts MCU power, docs/lume/
// 12-rtc-e-solo-x3.md §1.8). So the corner stamp says when the frame was
// drawn — honest, and it tells you how stale the list under it is — instead of
// a clock that would be wrong the moment you look at it.
// ---------------------------------------------------------------------------
bool nextTimedEvent(TodayStore::Item& out) {
  for (std::size_t i = 0; i < TODAY_STORE.count(); i++) {
    if (!TODAY_STORE.get(i, out)) continue;
    if (!out.time[0] || strcmp(out.time, "All day") == 0) continue;
    return true;
  }
  return false;
}

void drawSleepHeader(Gfx& gfx, const SleepConfig& cfg, const char* defaultTitle) {
  const int w = gfx.width();
  const int leftX = 24;
  const int rightX = w - 24;
  const int y = 14;

  const char* title = (cfg.customTitle[0] != '\0') ? cfg.customTitle : defaultTitle;
  gfx.drawText(kFontSmall, leftX, y, title);

  // Telemetry string: [temp] [battery] [timestamp]
  char statusBuf[64] = {0};
  int pos = 0;

  if (cfg.showTemperature) {
    int16_t tenths = 0;
    if (Ds3231::readTemperatureTenthsC(tenths)) {
      pos += snprintf(statusBuf + pos, sizeof(statusBuf) - pos, "%d,%d C  ", tenths / 10, std::abs(tenths % 10));
    }
  }

  if (cfg.showBattery) {
    uint16_t soc = 0;
    if (BatteryGauge::readWord(BatteryGauge::kCmdStateOfCharge, soc) && soc <= 100) {
      pos += snprintf(statusBuf + pos, sizeof(statusBuf) - pos, "%u%%  ", static_cast<unsigned>(soc));
    }
  }

  if (cfg.showSleepTime) {
    char clock[16];
    if (clockFormatTime(clock, sizeof(clock))) {
      snprintf(statusBuf + pos, sizeof(statusBuf) - pos, L10N("asleep %s", "dorme %s"), clock);
    }
  }

  if (statusBuf[0] != '\0') {
    const int statW = gfx.textWidth(kFontSmall, statusBuf);
    gfx.drawText(kFontSmall, rightX - statW, y, statusBuf);
  }

  gfx.drawLine(leftX, 42, rightX, 42, 1, true);
}

void renderFaceDashboard(Gfx& gfx, const SleepConfig& cfg) {
  const int w = gfx.width();
  const int cx = w / 2;
  drawSleepHeader(gfx, cfg, "LUME");

  // Date hero
  char dateBuf[48] = {0};
  if (clockFormatShortDate(dateBuf, sizeof(dateBuf))) {
    gfx.drawTextCentered(kFontBold, cx, 56, dateBuf);
    gfx.fillRect(cx - 28, 90, 56, 2, true);
  }

  int curY = 104;

  // Next Calendar Event (if available and enabled)
  if (cfg.showNextEvent) {
    TodayStore::Item item;
    if (nextTimedEvent(item)) {
      const int boxX = 24;
      const int boxW = w - 48;
      const int boxH = 56;
      gfx.drawRoundedRect(boxX, curY, boxW, boxH, 6, 1, true);

      // Inverted time pill
      const int pillW = gfx.textWidth(kFontSmall, item.time) + 16;
      const int pillH = 22;
      gfx.fillRoundedRect(boxX + 12, curY + 8, pillW, pillH, 4, true);
      gfx.drawText(kFontSmall, boxX + 20, curY + 8, item.time, false);

      // Title & Subtitle
      gfx.drawText(kFontBold, boxX + 12 + pillW + 12, curY + 8, item.title);
      if (item.subtitle[0] != '\0') {
        gfx.drawText(kFontSmall, boxX + 12, curY + 34, item.subtitle);
      }
      curY += boxH + 16;
    }
  }

  // Priority Reminders List
  const std::size_t remCount = REMINDERS_STORE.count();
  const char* listName = REMINDERS_STORE.listCount() > 0 ? REMINDERS_STORE.listName(0) : "PROMEMORIA";
  char secHeader[64];
  if (remCount > 0) {
    snprintf(secHeader, sizeof(secHeader), "%s (%u)", listName, static_cast<unsigned>(remCount));
  } else {
    snprintf(secHeader, sizeof(secHeader), "%s", listName);
  }
  gfx.drawText(kFontSmall, 28, curY, secHeader);
  curY += 26;

  if (remCount > 0) {
    const std::size_t maxToShow = (curY > 160) ? 4 : 6;
    const std::size_t toDraw = std::min(remCount, maxToShow);
    for (std::size_t i = 0; i < toDraw; ++i) {
      RemindersStore::Item item;
      if (!REMINDERS_STORE.get(i, item)) break;

      // Checkbox
      gfx.drawRect(28, curY + 4, 16, 16, 2, true);

      // Title
      gfx.drawText(kFontRegular, 52, curY, item.title);

      // Due tag if present
      if (item.due[0] != '\0') {
        const int dueW = gfx.textWidth(kFontSmall, item.due);
        gfx.drawText(kFontSmall, w - 28 - dueW, curY + 2, item.due);
      }
      curY += 34;
    }
  } else {
    gfx.drawText(kFontRegular, 28, curY, L10N("No pending reminders", "Nessun promemoria in sospeso"));
    curY += 34;
  }

  // Bottom block/ambient line
  int slotY = gfx.height() - 96;
  if (!RemindersScene::renderDormantBlockLine(gfx, slotY) && cfg.showReadingStats) {
    const auto band = reader::ReadingStats::band();
    if (band.todayPages > 0 || band.streakDays > 0) {
      char statLine[64];
      snprintf(statLine, sizeof(statLine), L10N("%u pages today | %u day streak", "%u pag oggi | %u gg di fila"),
               static_cast<unsigned>(band.todayPages), static_cast<unsigned>(band.streakDays));
      gfx.drawTextCentered(kFontSmall, cx, slotY, statLine);
    }
  }

  gfx.drawTextCentered(kFontSmall, cx, gfx.height() - 48,
                       L10N("press power to wake", "premi accensione per risvegliare"));
}

void renderFaceReader(Gfx& gfx, const SleepConfig& cfg) {
  const int w = gfx.width();
  const int cx = w / 2;
  drawSleepHeader(gfx, cfg, L10N("READING", "IN LETTURA"));

  // Check last read book path from Preferences
  char bookPath[96] = {0};
  {
    Preferences prefs;
    if (prefs.begin(kPrefsNamespace, /*readOnly=*/true)) {
      prefs.getString("lastBook", bookPath, sizeof(bookPath));
      prefs.end();
    }
  }

  const char* base = bookPath[0] ? bookPath : "Lume E-Reader";
  const char* lastSlash = strrchr(base, '/');
  if (lastSlash) base = lastSlash + 1;
  char titleBuf[64] = {0};
  snprintf(titleBuf, sizeof(titleBuf), "%s", base);
  char* dot = strrchr(titleBuf, '.');
  if (dot && (strcmp(dot, ".epub") == 0 || strcmp(dot, ".EPUB") == 0)) {
    *dot = '\0';
  }
  for (char* p = titleBuf; *p; ++p) {
    if (*p == '_') *p = ' ';
  }

  gfx.drawTextCentered(kFontBold, cx, 80, titleBuf);
  gfx.fillRect(cx - 32, 116, 64, 2, true);

  // Reading Stats
  const auto band = reader::ReadingStats::band();
  char statsBanner[64];
  snprintf(statsBanner, sizeof(statsBanner), L10N("Today: %u pages  |  Streak: %u days", "Oggi: %u pagine  |  Serie: %u giorni"),
           static_cast<unsigned>(band.todayPages), static_cast<unsigned>(band.streakDays));
  gfx.drawTextCentered(kFontRegular, cx, 136, statsBanner);

  // 7-day spark dots
  const char* dayLabels[7] = {L10N("M", "L"), L10N("T", "M"), L10N("W", "M"), L10N("T", "G"),
                              L10N("F", "V"), L10N("S", "S"), L10N("S", "D")};
  const int dotSpacing = 36;
  const int dotStartX = cx - (3 * dotSpacing);
  const int dotY = 176;
  for (int d = 0; d < 7; ++d) {
    const int dx = dotStartX + d * dotSpacing;
    gfx.drawTextCentered(kFontSmall, dx, dotY - 18, dayLabels[d]);
    if (band.weekPages[d] > 0) {
      gfx.fillRoundedRect(dx - 5, dotY, 10, 10, 5, true);
    } else {
      gfx.drawRoundedRect(dx - 5, dotY, 10, 10, 5, 1, true);
    }
  }

  // Quote or Literary Note
  const char* quote = cfg.customQuote[0] ? cfg.customQuote
                                         : L10N("A reader lives a thousand lives before he dies.",
                                                 "Un lettore vive mille vite prima di morire.");
  const char* author = cfg.customAuthor[0] ? cfg.customAuthor
                                           : L10N("George R.R. Martin", "George R.R. Martin");

  const int quoteBoxY = 240;
  gfx.drawRoundedRect(28, quoteBoxY, w - 56, 260, 8, 1, true);

  gfx.drawText(kFontBold, 44, quoteBoxY + 16, L10N("\"", "\xC2\xAB"));
  gfx.drawTextWrapped(kFontRegular, 56, quoteBoxY + 28, quote, w - 112, 6, true);
  char authorLine[64];
  snprintf(authorLine, sizeof(authorLine), "-- %s", author);
  const int authorW = gfx.textWidth(kFontSmall, authorLine);
  gfx.drawText(kFontSmall, w - 56 - authorW - 10, quoteBoxY + 220, authorLine);

  if (cfg.showNextEvent) {
    RemindersScene::renderDormantFooter(gfx, gfx.height() - 96);
  }

  gfx.drawTextCentered(kFontSmall, cx, gfx.height() - 48,
                       L10N("press power to wake", "premi accensione per risvegliare"));
}

void renderFaceQuote(Gfx& gfx, const SleepConfig& cfg) {
  const int w = gfx.width();
  const int cx = w / 2;
  drawSleepHeader(gfx, cfg, L10N("QUOTE", "CITAZIONE"));

  char dateBuf[48] = {0};
  if (clockFormatShortDate(dateBuf, sizeof(dateBuf))) {
    gfx.drawTextCentered(kFontSmall, cx, 68, dateBuf);
    gfx.fillRect(cx - 24, 94, 48, 1, true);
  }

  const char* quote = cfg.customQuote[0] ? cfg.customQuote
                                         : L10N("Simplicity is the ultimate sophistication.",
                                                "La semplicita e la suprema sofisticazione.");
  const char* author = cfg.customAuthor[0] ? cfg.customAuthor
                                           : L10N("Leonardo da Vinci", "Leonardo da Vinci");

  const int quoteY = 160;
  gfx.drawTextCentered(kFontBold, cx, quoteY, L10N("\"", "\xC2\xAB"));
  gfx.drawTextWrapped(kFontBold, 48, quoteY + 36, quote, w - 96, 8, true);

  char authorLine[64];
  snprintf(authorLine, sizeof(authorLine), "-- %s", author);
  gfx.drawTextCentered(kFontRegular, cx, quoteY + 260, authorLine);

  if (cfg.showNextEvent) {
    RemindersScene::renderDormantFooter(gfx, gfx.height() - 96);
  }

  gfx.drawTextCentered(kFontSmall, cx, gfx.height() - 48,
                       L10N("press power to wake", "premi accensione per risvegliare"));
}

void renderFaceMinimal(Gfx& gfx, const SleepConfig& cfg) {
  const int w = gfx.width();
  const int cx = w / 2;
  drawSleepHeader(gfx, cfg, "LUME");

  // Center Lume Emblem
  drawLumeMark(gfx, cx, 190, 120);

  // Centered Wordmark
  gfx.drawTextCentered(kFontBold, cx, 330, "lume");
  gfx.fillRect(cx - 28, 368, 56, 2, true);

  // Date line
  char dateBuf[48] = {0};
  if (clockFormatShortDate(dateBuf, sizeof(dateBuf))) {
    gfx.drawTextCentered(kFontRegular, cx, 396, dateBuf);
  }

  if (cfg.showNextEvent) {
    RemindersScene::renderDormantFooter(gfx, gfx.height() - 96);
  }

  gfx.drawTextCentered(kFontSmall, cx, gfx.height() - 48,
                       L10N("press power to wake", "premi accensione per risvegliare"));
}

void drawSleepScreen(Gfx& gfx) {
  gfx.clear();
  SLEEP_CONFIG.load();
  const SleepConfig& cfg = SLEEP_CONFIG.config();

  SleepFace activeFace = cfg.face;
  if (activeFace == SleepFace::Auto) {
    if (gCurrentSceneId == SceneId::Workout && WorkoutScene::renderDormant(gfx)) {
      int slotY = gfx.height() - 108;
      if (RemindersScene::renderDormantBlockLine(gfx, slotY)) slotY = gfx.height() - 148;
      RemindersScene::renderDormantFooter(gfx, slotY);
      gfx.drawTextCentered(kFontSmall, gfx.width() / 2, gfx.height() - 56,
                           L10N("press power to wake", "premi accensione"));
      gfx.display().requestResync(1);
      gfx.flush(EInkDisplay::FULL_REFRESH);
      return;
    } else if (gCurrentSceneId == SceneId::Reader) {
      activeFace = SleepFace::Reader;
    } else {
      activeFace = SleepFace::Dashboard;
    }
  }

  switch (activeFace) {
    case SleepFace::Dashboard:
      renderFaceDashboard(gfx, cfg);
      break;
    case SleepFace::Reader:
      renderFaceReader(gfx, cfg);
      break;
    case SleepFace::Reminders:
      if (!RemindersScene::renderDormant(gfx)) {
        renderFaceDashboard(gfx, cfg);
      }
      break;
    case SleepFace::Quote:
      renderFaceQuote(gfx, cfg);
      break;
    case SleepFace::Minimal:
      renderFaceMinimal(gfx, cfg);
      break;
    default:
      renderFaceDashboard(gfx, cfg);
      break;
  }

  gfx.display().requestResync(1);
  gfx.flush(EInkDisplay::FULL_REFRESH);
}

// ---------------------------------------------------------------------------
// QMI8658 IMU (X3 only): xphone-os never initializes the tilt sensor, so at
// this point the chip still sits in its power-on default — CTRL1 bit0
// (sensorDisable) is 0, i.e. the internal 2 MHz oscillator runs. x4-os
// disables it explicitly at every boot (HalTiltSensor.cpp:74-84 "initialized
// and put to sleep"); replicate those two register writes here in the sleep
// path (minimal I2C, no SDK link — x4-os is read-only reference). Registers/
// addresses: HalTiltSensor.h:44-64, HalGPIO.h:35-39.
// ---------------------------------------------------------------------------
// Lume targets X3 only; always called before power-off so the QMI8658
// oscillator is not left running.
constexpr uint8_t kImuAddr = 0x6B;     // I2C_ADDR_QMI8658
constexpr uint8_t kImuAddrAlt = 0x6A;  // I2C_ADDR_QMI8658_ALT
constexpr uint8_t kImuWhoAmIReg = 0x00;
constexpr uint8_t kImuWhoAmIValue = 0x05;
constexpr uint8_t kImuRegCtrl1 = 0x02;
constexpr uint8_t kImuRegCtrl7 = 0x08;
// CTRL1 = AUTO_INC | BIG_ENDIAN | SENSOR_DISABLE; CTRL7 = disable all engines.
constexpr uint8_t kImuCtrl1Sleep = 0x61;
constexpr uint8_t kImuCtrl7Sleep = 0x00;

bool imuWriteReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool imuReadReg(uint8_t addr, uint8_t reg, uint8_t& val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;  // repeated start
  if (Wire.requestFrom(addr, static_cast<uint8_t>(1)) < 1) return false;
  val = static_cast<uint8_t>(Wire.read());
  return true;
}

void imuSleep() {
  // Same bus as the BQ27220 gauge; TwoWire::begin() is idempotent
  // (BatteryGauge.cpp busAddr() pattern).
  const auto& g = BoardConfig::ACTIVE.batteryGauge;
  Wire.begin(g.i2cSda, g.i2cScl, g.i2cHz);

  // Probe primary then alternate address, exactly like HalTiltSensor::begin().
  uint8_t addr = kImuAddr;
  uint8_t whoami = 0;
  if (!imuReadReg(addr, kImuWhoAmIReg, whoami) || whoami != kImuWhoAmIValue) {
    addr = kImuAddrAlt;
    if (!imuReadReg(addr, kImuWhoAmIReg, whoami) || whoami != kImuWhoAmIValue) {
      Serial.println("[xphone-os] sleep: QMI8658 not found, skipping IMU sleep");
      return;
    }
  }
  if (imuWriteReg(addr, kImuRegCtrl7, kImuCtrl7Sleep) && imuWriteReg(addr, kImuRegCtrl1, kImuCtrl1Sleep)) {
    Serial.printf("[xphone-os] sleep: QMI8658 @0x%02X put to sleep\n", addr);
  } else {
    Serial.println("[xphone-os] sleep: QMI8658 register write failed");
  }
}
}  // namespace

namespace Sleep {

void sleepNow(Gfx& gfx, Input& input) {
  // M5 Phase 1/2: the flush worker must be idle before this function's direct
  // panel paints, and the sampling task must stop before deep-sleep teardown
  // (wake is a full power-on reset, so no resume needed).
  WorkoutScene::flushPendingSend();  // coalesced workout.set must not die with the loop
  // Same rationale, higher stakes: an in-progress DAILY board that dies with the
  // loop is the one thing the player cannot regenerate — tomorrow hands out a
  // different puzzle. No-op unless a game scene is on glass.
  gamesPersistDaily();
  SCENES.waitFlushIdle();
  input.suspendTask();
  Serial.println("[xphone-os] entering deep sleep");

  // M4.2: persist the scene on glass to NVS before anything else so boot()
  // restores it on wake (each scene re-requests its data onEnter). Any scene —
  // not just Block — is restored. NVS survives the X3's power-button power-on
  // reset; the key is consumed (removed) at boot, so a cold boot with no prior
  // sleep finds no key and lands on the launcher. The ~10-20ms write is fine:
  // we already wait ~1.5s for the priorities sync above.
  {
    Preferences prefs;
    if (prefs.begin(kPrefsNamespace, /*readOnly=*/false)) {
      prefs.putUInt(kPrefsSceneKey, static_cast<uint32_t>(gCurrentSceneId));

      // M4.3: persist a tiny Block snapshot so wake-from-active-block shows the
      // locked view instantly (seedPersistedBlock at boot). Absolute end-time
      // label only — no minute countdown: the block's end is a display string,
      // so wall time alone cannot rebuild it (see BlockScene).
      // Clear the keys when no block is running so a stale snapshot never
      // resurrects a finished block.
      const BlockStatusStore::Status blk = BLOCK_STATUS.get();
      if (blk.active) {
        prefs.putBool(kBlkActiveKey, true);
        prefs.putBool(kBlkBreakKey, blk.onBreak);
        prefs.putInt(kBlkRemainKey, blk.remainingMinutes);
        prefs.putInt(kBlkDurKey, blk.durationMinutes);
        prefs.putString(kBlkPresetKey, blk.preset);
        prefs.putString(kBlkEndsKey, blk.endsAtLabel);
      } else {
        prefs.remove(kBlkActiveKey);
        prefs.remove(kBlkBreakKey);
        prefs.remove(kBlkRemainKey);
        prefs.remove(kBlkDurKey);
        prefs.remove(kBlkPresetKey);
        prefs.remove(kBlkEndsKey);
      }
      // Completion counters persist regardless of active state.
      prefs.putInt(kBlkTodayKey, blk.blocksToday);
      prefs.putInt(kBlkStreakKey, blk.streak);
      prefs.putInt(kBlkTotalKey, blk.total);

      // Last Today / Priorities snapshot JSON so the dormant/wake render shows
      // cached data instead of the blank "Syncing" screen (seeded at boot).
      const std::string& todayJson = COMPANION_BLE.getLastTodayCard();
      if (!todayJson.empty()) prefs.putString(kTodayCardKey, todayJson.c_str());
      // Reminders: serialized from the STORE, not the last phone card — a
      // multi-part snapshot's final card only carries the tail slice, so the
      // raw payload no longer represents the whole list.
      if (REMINDERS_STORE.count() > 0) {
        JsonDocument doc;
        doc["kind"] = "reminders.snapshot";
        doc["id"] = "rem-persist";
        doc["body"] = REMINDERS_STORE.syncLine();
        doc["gen"] = REMINDERS_STORE.generation();
        JsonArray lists = doc["reminderLists"].to<JsonArray>();
        for (std::size_t i = 0; i < REMINDERS_STORE.listCount(); i++) {
          JsonArray row = lists.add<JsonArray>();
          row.add(i);
          row.add(REMINDERS_STORE.listName(i));
        }
        JsonArray items = doc["reminderItems"].to<JsonArray>();
        RemindersStore::Item it;
        for (std::size_t i = 0; REMINDERS_STORE.get(i, it); i++) {
          JsonArray row = items.add<JsonArray>();
          row.add(it.handle);
          row.add(it.list);
          row.add(it.title);
          row.add(it.due);
        }
        String out;
        serializeJson(doc, out);
        prefs.putString(kRemCardKey, out);
      }

      // Workout snapshot: serialized from the STORE, not the last phone card —
      // sets counted while the phone was away live only in the store, and the
      // wake seed must not regress them.
      if (WORKOUT_STORE.count() > 0) {
        JsonDocument doc;
        doc["kind"] = "workout.snapshot";
        doc["id"] = "workout-persist";
        doc["workoutDate"] = WORKOUT_STORE.date();
        JsonArray items = doc["workoutItems"].to<JsonArray>();
        WorkoutStore::Item it;
        for (std::size_t i = 0; WORKOUT_STORE.get(i, it); i++) {
          JsonArray row = items.add<JsonArray>();
          row.add(it.id);
          row.add(it.name);
          row.add(it.sets);
          row.add(it.done);
        }
        String out;
        serializeJson(doc, out);
        prefs.putString(kWorkoutCardKey, out);
      }

      // Newest notifications (shared persistence scratch, declared above).
      {
        const std::size_t n = NOTIFICATION_STORE.snapshot(gNotifScratch, sizeof(gNotifScratch));
        if (n > 0) prefs.putBytes(kNotifStoreKey, gNotifScratch, n);
        else prefs.remove(kNotifStoreKey);  // do not resurrect a list cleared since the last sleep

        // Individual-clear tombstones are a separate raw blob so changing the
        // notification snapshot policy cannot discard the replay retry memory.
        const std::size_t tombN = NOTIFICATION_STORE.tombstoneSnapshot(gTombScratch, sizeof(gTombScratch));
        if (tombN > 0) prefs.putBytes(kNotifTombKey, gTombScratch, tombN);
        else prefs.remove(kNotifTombKey);
      }
      prefs.end();
    } else {
      Serial.println("[xphone-os] sleep: NVS open failed; scene restore disabled this cycle");
    }
  }

  // 0. Best-effort priorities refresh so the dormant frame holds TONIGHT's
  //    list: the iPhone never pushes a snapshot unsolicited (it only answers
  //    priorities.sync.request / priority.toggle, or the user taps "Send to
  //    X4"), so ask now and wait for the reply. Because resync is now
  //    scene-scoped (only the on-glass scene's rail syncs on connect), the
  //    priorities store is often empty here if the user never opened the
  //    Priorities app this session — so this pre-sleep fetch is the ONLY way
  //    the dormant frame gets a list. The wait must pump processPending()
  //    itself — card JSON parses ONLY on the main loop (handleCardWrite just
  //    stashes raw bytes) and this function never returns to loop(). The
  //    ceiling is 3500 ms (not 1500): a round trip on the low-duty link
  //    (180 ms interval, slave latency 4) is two connection events ~= 1.8 s+
  //    plus the phone's processing, so 1500 ms often missed. It BREAKS EARLY
  //    the moment the snapshot lands, so the common case still sleeps fast;
  //    3500 ms is just the ceiling for a slow link. Phone disconnected ->
  //    skip; no reply in time -> render whatever the store holds (empty ->
  //    wordmark fallback).
  if (COMPANION_BLE.isConnected()) {
    const uint32_t rev0 = REMINDERS_STORE.revision();
    if (COMPANION_BLE.sendRemindersSyncRequest()) {
      const unsigned long tRequest = millis();
      while (millis() - tRequest < 3500UL) {
        COMPANION_BLE.processPending();
        if (REMINDERS_STORE.revision() != rev0) break;  // fresh snapshot landed
        delay(25);
      }
    }
    // M5: same best-effort refresh for the Today snapshot — the footer shows
    // the NEXT calendar event, so it must be as fresh as the priorities list.
    // Sent AFTER the priorities reply (or its timeout): both commands notify
    // on the shared action characteristic, and back-to-back notifies clobber.
    const uint32_t todayRev0 = TODAY_STORE.revision();
    if (COMPANION_BLE.sendTodaySyncRequest()) {
      const unsigned long tRequest = millis();
      while (millis() - tRequest < 3500UL) {
        COMPANION_BLE.processPending();
        if (TODAY_STORE.revision() != todayRev0) break;  // fresh snapshot landed
        delay(25);
      }
    }
  }

  // 1. Sleep screen on glass first (FULL refresh) — everything after this is
  //    invisible teardown, so the device *feels* asleep immediately.
  drawSleepScreen(gfx);

  // 2. BLE: stop advertising cleanly. Full stack teardown is left to the
  //    deep-sleep chip reset (bonds live in NVS and survive; a connected
  //    iPhone drops the link at its supervision timeout and resumes from the
  //    bond on wake — same approach as x4-os, which never deinits BLE before
  //    powerManager.startDeepSleep()).
  if (COMPANION_BLE.isStarted()) {
    COMPANION_BLE.stopAdvertising();
  }

  // 3. Stop the X3 QMI8658 internal oscillator.
  imuSleep();

  // 4. Panel controller into deep sleep, holding the sleep screen — the only
  //    panel deepSleep() in the OS under the M4 two-state power model (x4-os
  //    does the same right before its ESP sleep, src/main.cpp:268).
  gfx.display().deepSleep();

  // 5. Guard: wait for the power button to be fully RELEASED. sleepNow() is
  //    triggered on the release edge so this normally falls straight through,
  //    but the C3 GPIO wakeup is LEVEL-triggered (LOW) — a still-low pin at
  //    esp_deep_sleep_start() would wake instantly. Debounced state first
  //    (x4-os HalPowerManager.cpp:64-68), then the raw pin level.
  while (input.powerPressed()) {
    delay(50);
    input.update();
  }
  pinMode(InputManager::POWER_BUTTON_PIN, INPUT_PULLUP);
  while (digitalRead(InputManager::POWER_BUTTON_PIN) == LOW) {
    delay(10);
  }

  Serial.println("[xphone-os] deep sleep armed; good night");
  Serial.flush();
  // Tear down the USB Serial/JTAG CDC so the host sees a clean disconnect and
  // the peripheral doesn't hold power domains that interfere with USB-powered
  // GPIO wake (x4-os HalPowerManager.cpp:70-77).
  Serial.end();

  // 6. Pre-sleep routines from the original firmware (HalPowerManager.cpp:
  //    79-86): GPIO13 drives the battery latch MOSFET and must be held low
  //    during sleep — on battery the MCU is then completely powered off and
  //    the power button hard-wires a power-up regardless of the wakeup
  //    source below.
  constexpr gpio_num_t kBatteryLatchPin = GPIO_NUM_13;
  gpio_set_direction(kBatteryLatchPin, GPIO_MODE_OUTPUT);
  gpio_set_level(kBatteryLatchPin, 0);
  esp_sleep_config_gpio_isolate();
  gpio_deep_sleep_hold_en();
  gpio_hold_en(kBatteryLatchPin);

  // 7. Arm the wakeup and go. ESP32-C3 has no ext0/ext1 — the deep-sleep
  //    "gpio" source takes a pin bitmask + level (active-low button =>
  //    ESP_GPIO_WAKEUP_GPIO_LOW; x4-os HalPowerManager.cpp:92, freeink-sdk
  //    PowerManager.cpp:30). On USB power this is the only wake path, so log
  //    a failure — sleeping regardless is still safe on battery (hard-wired
  //    power-button latch above).
  const esp_err_t wakeErr =
      esp_deep_sleep_enable_gpio_wakeup(1ULL << InputManager::POWER_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  if (wakeErr != ESP_OK) {
    // Serial is already down; nothing more we can report. Restart instead of
    // sleeping unwakeable-on-USB.
    esp_restart();
  }
  esp_deep_sleep_start();
  while (true) {  // esp_deep_sleep_start() does not return; satisfy [[noreturn]]
    delay(1000);
  }
}

void armRestoreScene(uint32_t sceneId) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, /*readOnly=*/false)) return;
  prefs.putUInt(kPrefsSceneKey, sceneId);
  prefs.end();
}

bool consumeRestoreScene(uint32_t& sceneId) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, /*readOnly=*/false)) return false;
  bool found = false;
  if (prefs.isKey(kPrefsSceneKey)) {
    sceneId = prefs.getUInt(kPrefsSceneKey, 0);
    prefs.remove(kPrefsSceneKey);  // consume-once: next cold boot has no key
    found = true;
  }
  prefs.end();
  return found;
}

void seedPersistedBlock() {
  Preferences prefs;
  // Read-only open fails when the namespace has never been written (first-ever
  // cold boot) — behave as today (no seed) in that case.
  if (!prefs.begin(kPrefsNamespace, /*readOnly=*/true)) return;
  if (prefs.isKey(kBlkActiveKey) && prefs.getBool(kBlkActiveKey, false)) {
    const bool onBreak = prefs.getBool(kBlkBreakKey, false);
    const int remaining = prefs.getInt(kBlkRemainKey, 0);
    const int duration = prefs.getInt(kBlkDurKey, 0);
    char preset[24] = {0};
    char ends[16] = {0};
    prefs.getString(kBlkPresetKey, preset, sizeof(preset));
    prefs.getString(kBlkEndsKey, ends, sizeof(ends));
    BLOCK_STATUS.seedFromPersisted(true, onBreak, remaining, duration, preset, ends);
    Serial.printf("[xphone-os] boot: seeded active Block snapshot (%s, until %s)\n", preset[0] ? preset : "Block",
                  ends[0] ? ends : "?");
  }
  // Completion counters seed regardless of active state (today's count shows on
  // the dormant frame / Block scene even when no block is currently running).
  BLOCK_STATUS.seedCounts(prefs.getInt(kBlkTodayKey, 0), prefs.getInt(kBlkStreakKey, 0),
                          prefs.getInt(kBlkTotalKey, 0));

  // Re-seed the last Today / Priorities snapshots so those scenes render cached
  // data on wake (with a "syncing" indicator) instead of a blank sync screen.
  const String todayJson = prefs.getString(kTodayCardKey, "");
  if (todayJson.length() > 0) COMPANION_BLE.seedPersistedCard(std::string(todayJson.c_str()));
  // Reminders seed; also cleans the obsolete prioCard key once if present.
  if (prefs.isKey("prioCard")) prefs.remove("prioCard");
  const String remJson = prefs.getString(kRemCardKey, "");
  if (remJson.length() > 0) COMPANION_BLE.seedPersistedCard(std::string(remJson.c_str()));
  const String workoutJson = prefs.getString(kWorkoutCardKey, "");
  if (workoutJson.length() > 0) COMPANION_BLE.seedPersistedCard(std::string(workoutJson.c_str()));

  // Last-known notifications: instant list on wake; the ANCS backfill then
  // refreshes/dedupes into it (NotificationStore::restore contract).
  if (prefs.isKey(kNotifStoreKey)) {
    const std::size_t n = prefs.getBytes(kNotifStoreKey, gNotifScratch, sizeof(gNotifScratch));
    if (n >= sizeof(NotificationStore::Entry) && n % sizeof(NotificationStore::Entry) == 0) {
      NOTIFICATION_STORE.restore(gNotifScratch, n);
      Serial.printf("[xphone-os] boot: seeded %u persisted notification(s)\n",
                    static_cast<unsigned>(n / sizeof(NotificationStore::Entry)));
    } else if (n > 0) {
      Serial.printf("[xphone-os] boot: ignored stale notification snapshot (%u bytes)\n",
                    static_cast<unsigned>(n));
    }
  }

  // Tombstones survive the sleep window between a local clear and iOS's next
  // replay. Their raw format is independently size-validated for clean firmware
  // upgrades, exactly like Entry above.
  if (prefs.isKey(kNotifTombKey)) {
    const std::size_t n = prefs.getBytes(kNotifTombKey, gTombScratch, sizeof(gTombScratch));
    if (n >= sizeof(NotificationStore::Tombstone) && n % sizeof(NotificationStore::Tombstone) == 0) {
      NOTIFICATION_STORE.restoreTombstones(gTombScratch, n);
      Serial.printf("[xphone-os] boot: seeded %u notification tombstone(s)\n",
                    static_cast<unsigned>(n / sizeof(NotificationStore::Tombstone)));
    }
  }
  prefs.end();
}

}  // namespace Sleep
