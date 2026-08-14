#include "AboutScene.h"

#include <Arduino.h>
#include <BatteryMonitor.h>
#include <BoardConfig.h>

#include <cstdio>

#include "../BatteryGauge.h"
#include "../ClockStore.h"
#include "../Ds3231.h"
#include "../Fonts.h"
#include "../NotificationStore.h"
#include "../Sleep.h"
#include "../ble/CompanionAncsClient.h"
#include "../ble/CompanionBleService.h"
#include "AppScenes.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

void AboutScene::handleInput(Input& in) {
  if (in.wasPressed(Btn::Back)) showLauncher();
}

void AboutScene::render(Gfx& gfx) {
  // Values are sampled here, once per entry — the scene renders only when
  // dirty, so this is a snapshot, not a live monitor (e-ink discipline).
  char line[96];
  const int x = 24;
  int y = 16;

  gfx.drawText(kFontBold, x, y, L10N("About Lume", "Informazioni su Lume"));
  y += gfx.lineHeight(kFontBold) + 10;
  gfx.fillRect(x, y - 6, gfx.width() - 2 * x, 2, true);

  snprintf(line, sizeof(line), L10N("version: %s (built %s %s)", "versione: %s (creata %s %s)"), XPHONE_VERSION, __DATE__, __TIME__);
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  snprintf(line, sizeof(line), L10N("panel: %s  %dx%d", "pannello: %s  %dx%d"), BoardConfig::ACTIVE.name, gfx.width(), gfx.height());
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  snprintf(line, sizeof(line), L10N("boot to first paint: %lu ms", "avvio al primo disegno: %lu ms"), gBootTotalMs);
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  // M4.2 wake diagnostic: reset reason + last-scene restore outcome, captured
  // once in boot(). Tells us whether the power-button wake is DEEPSLEEP (RTC
  // would survive) or POWERON (needs NVS), and whether a scene was restored.
  snprintf(line, sizeof(line), L10N("wake: %s  restore: %s", "risveglio: %s  ripristino: %s"), gWakeResetReason, gWakeRestoreScene);
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  // v0.4 clock readout: what the device believes the local time is, where that
  // came from, and what the DS3231 itself reports. `rtc: OSF` after a power-off
  // is the on-glass verdict on the backup cell — the whole reason the hardware
  // clock can be trusted or not (docs/lume/12-rtc-e-solo-x3.md §1.1).
  {
    char clock[16];
    char date[20];
    const bool haveClock = clockFormatTime(clock, sizeof(clock)) && clockFormatShortDate(date, sizeof(date));

    char rtcState[24];
    bool stopped = false;
    int16_t tenthsC = 0;
    if (!Ds3231::oscillatorStopped(stopped)) {
      snprintf(rtcState, sizeof(rtcState), L10N("absent", "assente"));
    } else if (stopped) {
      snprintf(rtcState, sizeof(rtcState), L10N("OSF (time lost)", "OSF (ora persa)"));
    } else if (Ds3231::readTemperatureTenthsC(tenthsC)) {
      snprintf(rtcState, sizeof(rtcState), "ok  %d.%u C", tenthsC / 10,
               static_cast<unsigned>(tenthsC < 0 ? -tenthsC % 10 : tenthsC % 10));
    } else {
      snprintf(rtcState, sizeof(rtcState), "ok");
    }

    if (haveClock) {
      snprintf(line, sizeof(line), L10N("clock: %s %s (%s)  rtc: %s", "orologio: %s %s (%s)  rtc: %s"), clock, date,
               CLOCK_STORE.fromRtc ? "rtc" : "iPhone", rtcState);
    } else {
      snprintf(line, sizeof(line), L10N("clock: not set  rtc: %s", "orologio: non impostato  rtc: %s"), rtcState);
    }
  }
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  // Phase 0 morning-meditation spec: wake→BLE-connect and wake→date latency
  // (millis() starts ~0 at boot, so the stamps ARE the delays). This readout
  // is the experiment — no serial cable needed.
  if (CLOCK_STORE.firstPhoneSyncMs != 0) {
    snprintf(line, sizeof(line),
             L10N("time.sync: ble %lu ms  date %lu ms", "time.sync: ble %lu ms  data %lu ms"),
             static_cast<unsigned long>(CLOCK_STORE.firstConnectMs),
             static_cast<unsigned long>(CLOCK_STORE.firstPhoneSyncMs));
  } else if (CLOCK_STORE.firstConnectMs != 0) {
    snprintf(line, sizeof(line), L10N("time.sync: ble %lu ms  date pending",
                                     "time.sync: ble %lu ms  data in attesa"),
             static_cast<unsigned long>(CLOCK_STORE.firstConnectMs));
  } else {
    snprintf(line, sizeof(line), L10N("time.sync: no connect since boot", "time.sync: nessuna connessione dall'avvio"));
  }
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  snprintf(line, sizeof(line), L10N("heap free: %u B", "heap libera: %u B"), static_cast<unsigned>(ESP.getFreeHeap()));
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  snprintf(line, sizeof(line), L10N("heap min free: %u B", "heap libera minima: %u B"),
           static_cast<unsigned>(esp_get_minimum_free_heap_size()));
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  snprintf(line, sizeof(line), L10N("heap largest block: %u B", "blocco heap maggiore: %u B"),
           static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 10;

  // --- M2.1a: refresh instrumentation — the LAST interaction before entering
  // About (entering About is itself a FULL scene-switch refresh that
  // overwrites gRefreshStats after this render, so what is on glass here is
  // the interaction just before the switch). No serial cable needed: this IS
  // the readout. ------------------------------------------------------------
  gfx.fillRect(x, y - 6, gfx.width() - 2 * x, 2, true);

  snprintf(line, sizeof(line), L10N("last draw: %lu ms", "ultimo disegno: %lu ms"), gRefreshStats.drawMs);
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  snprintf(line, sizeof(line), L10N("last refresh: %lu ms (%s)", "ultimo refresh: %lu ms (%s)"), gRefreshStats.refreshMs, gRefreshStats.tier);
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  snprintf(line, sizeof(line), L10N("partials since scrub: %u  (HALF every %u)", "parziali dal reset: %u  (HALF ogni %u)"),
           static_cast<unsigned>(gRefreshStats.sinceScrub), static_cast<unsigned>(kScrubAfterRefreshes));
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 10;

  // --- M2.1b: power instrumentation — About IS the meter readout for the
  // power levers (uptime + gauge average current = drain per configuration;
  // adv mode + conn interval = what the radio is actually doing). -----------
  gfx.fillRect(x, y - 6, gfx.width() - 2 * x, 2, true);

  snprintf(line, sizeof(line), L10N("cpu: %lu MHz  uptime: %lu min", "cpu: %lu MHz  attivo: %lu min"),
           static_cast<unsigned long>(getCpuFrequencyMhz()), static_cast<unsigned long>(millis() / 60000UL));
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  {
    // BoardConfig-driven backend: X3 = BQ27220 I2C gauge, X4 = ADC divider.
    static BatteryMonitor battery;
    const BatteryMonitor::Status batt = battery.readStatus();
    int16_t avgMa = 0;
    const bool hasAvg = BatteryGauge::readAvgCurrentMa(avgMa);
    if (batt.supported && batt.percentageKnown && batt.millivoltsKnown) {
      if (hasAvg) {
        snprintf(line, sizeof(line), L10N("battery: %u%%  %u mV  avg %d mA", "batteria: %u%%  %u mV  media %d mA"), batt.percentage, batt.millivolts, avgMa);
      } else {
        snprintf(line, sizeof(line), L10N("battery: %u%%  %u mV", "batteria: %u%%  %u mV"), batt.percentage, batt.millivolts);
      }
    } else {
      snprintf(line, sizeof(line), L10N("battery: unavailable", "batteria: non disponibile"));
    }
  }
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  {
    // M2.1b follow-up: gauge capacity readout — remaining / full-charge /
    // design, all mAh (BQ27220 std commands 0x10 / 0x12 / 0x3C). This is the
    // on-glass check that the boot-time design-capacity reprogram took (see
    // BatteryGauge::ensureDesignCapacity): design must read 650, not the
    // factory 3000. X3 only — on X4 (ADC path) readWord() returns false and
    // the line is skipped, leaving the existing battery lines unchanged.
    uint16_t remainMah = 0, fccMah = 0, designMah = 0;
    if (BatteryGauge::readWord(BatteryGauge::kCmdRemainingCapacity, remainMah) &&
        BatteryGauge::readWord(BatteryGauge::kCmdFullChargeCapacity, fccMah) &&
        BatteryGauge::readWord(BatteryGauge::kCmdDesignCapacity, designMah)) {
      snprintf(line, sizeof(line), L10N("gauge: %u/%u mAh  design %u mAh", "gauge: %u/%u mAh  nominale %u mAh"), remainMah, fccMah, designMah);
      gfx.drawText(kFontRegular, x, y, line);
      y += gfx.lineHeight(kFontRegular) + 4;
    }
  }

  if (!COMPANION_BLE.isStarted()) {
    snprintf(line, sizeof(line), L10N("adv: off", "adv: spento"));
  } else if (COMPANION_BLE.isConnected()) {
    snprintf(line, sizeof(line), L10N("adv: stopped (connected)", "adv: fermo (connesso)"));
  } else if (COMPANION_BLE.getAdvMode() == CompanionBleService::AdvMode::Fast) {
    snprintf(line, sizeof(line), L10N("adv: fast (30-60 ms window)", "adv: veloce (finestra 30-60 ms)"));
  } else {
    snprintf(line, sizeof(line), L10N("adv: slow (400-500 ms)", "adv: lento (400-500 ms)"));
  }
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  {
    uint16_t itvl125 = 0, latency = 0, timeout10ms = 0;
    if (COMPANION_BLE.getConnParams(itvl125, latency, timeout10ms)) {
      const unsigned itvlUs = static_cast<unsigned>(itvl125) * 1250u;  // 1.25 ms units
      snprintf(line, sizeof(line),
               L10N("conn: %u.%02u ms  lat=%u  timeout=%u ms",
                    "conn: %u.%02u ms  lat=%u  timeout=%u ms"),
               itvlUs / 1000u, (itvlUs % 1000u) / 10u, latency,
               static_cast<unsigned>(timeout10ms) * 10u);
    } else {
      snprintf(line, sizeof(line), L10N("conn: none", "conn: nessuna"));
    }
  }
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 10;

  // --- M2: companion BLE / ANCS snapshot (same e-ink discipline: sampled
  // once per render, no live polling) --------------------------------------
  gfx.fillRect(x, y - 6, gfx.width() - 2 * x, 2, true);

  if (!COMPANION_BLE.isStarted()) {
    snprintf(line, sizeof(line), L10N("ble: off", "ble: spento"));
  } else if (COMPANION_BLE.isConnected()) {
    snprintf(line, sizeof(line), L10N("ble: connected", "ble: connesso"));
  } else {
    snprintf(line, sizeof(line), L10N("ble: advertising as %s", "ble: visibile come %s"), CompanionProtocol::deviceName());
  }
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  char peer[18];
  if (COMPANION_BLE.getPeerAddress(peer, sizeof(peer))) {
    snprintf(line, sizeof(line), L10N("peer: %s%s", "dispositivo: %s%s"), peer, COMPANION_BLE.isConnected() ? "" : L10N(" (last)", " (ultimo)"));
  } else {
    snprintf(line, sizeof(line), L10N("peer: none since boot", "dispositivo: nessuno dall'avvio"));
  }
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  // ANCS status snapshot (fixed-buffer copy under the client's state mutex).
  char ancsStatus[48];
  COMPANION_ANCS.getStatusMessage(ancsStatus, sizeof(ancsStatus));
  snprintf(line, sizeof(line), "ancs: %s", ancsStatus);
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  snprintf(line, sizeof(line), L10N("notifications stored: %u", "notifiche salvate: %u"),
           static_cast<unsigned>(NOTIFICATION_STORE.count()));
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

  // M2.1d: stack + ANCS-queue audit on glass, so hardware validation needs
  // no serial cable. HWM values are bytes (ESP-IDF StackType_t is uint8_t).
  {
    const UBaseType_t loopHwm = uxTaskGetStackHighWaterMark(nullptr);
    const TaskHandle_t bleTask = COMPANION_ANCS.getHostTaskHandle();
    if (bleTask) {
      snprintf(line, sizeof(line),
               L10N("stack HWM: loop %u B  ble %u B  ancs q peak %u/6 drops %lu",
                    "stack HWM: loop %u B  ble %u B  coda ancs max %u/6 persi %lu"),
               static_cast<unsigned>(loopHwm), static_cast<unsigned>(uxTaskGetStackHighWaterMark(bleTask)),
               COMPANION_ANCS.getQueueHighWater(),
               static_cast<unsigned long>(COMPANION_ANCS.getQueueDropCount()));
    } else {
      snprintf(line, sizeof(line),
               L10N("stack HWM: loop %u B  ble n/a  ancs q peak %u/6 drops %lu",
                    "stack HWM: loop %u B  ble n/d  coda ancs max %u/6 persi %lu"),
               static_cast<unsigned>(loopHwm), COMPANION_ANCS.getQueueHighWater(),
               static_cast<unsigned long>(COMPANION_ANCS.getQueueDropCount()));
    }
  }
  gfx.drawText(kFontRegular, x, y, line);
  y += gfx.lineHeight(kFontRegular) + 4;

#if XP_AUTO_SLEEP_MS > 0
  snprintf(line, sizeof(line), L10N("Press power to sleep (auto %lu min), hold 3s to restart", "Premi accensione per dormire (auto %lu min), tieni 3s per riavviare"),
           static_cast<unsigned long>(XP_AUTO_SLEEP_MS / 60000UL));
#else
  snprintf(line, sizeof(line), L10N("Press power to sleep, hold 3s to restart", "Premi accensione per dormire, tieni 3s per riavviare"));
#endif
  gfx.drawText(kFontRegular, x, y, line);
  // M2.1: footer hint replaced by the SceneManager soft-key bar (default BACK tab).
}
