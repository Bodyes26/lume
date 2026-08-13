# 12 — Orologio hardware DS3231 e fork solo-X3

Due analisi di fattibilità sul codice reale. Legenda: **FATTO** = verificato leggendo il file citato (`percorso:riga`); **[INFERENZA]** = deduzione non verificabile senza hardware o build. Nessuna build/lint/test eseguita, `flowe-os/` non modificato.

---

## 1. Orologio hardware DS3231 sull'X3

### 1.1 Cosa esiste già, e la contraddizione da cui partire

**FATTO — il chip c'è.** Il DS3231 è interrogato a I²C `0x68` come *impronta digitale* per distinguere X3 da X4: `XteinkDetect.cpp:17` (`ADDR_DS3231 = 0x68`), `:23` (`DS3231_SEC_REG = 0x00`), `:60-66` (`probeDs3231()` legge il registro 0 e valida i secondi BCD, `tens <= 5 && ones <= 9`). Il punteggio entra in `runProbePass()` a `:109-110`; `detectXteinkIsX3()` a `:119-127` richiede ≥2 chip su 3 in due passate. Documentato anche in `XteinkDetect/library.json:4` e `XteinkDetect/include/XteinkDetect.h:12-15`.

**FATTO — il resto del firmware crede che l'RTC non esista.** Tutte le decisioni di prodotto sono costruite sull'assunto opposto: `xphone-os/src/ClockStore.h:3-8` ("this hardware has no RTC and the X3 power-button wake wipes software time. The iOS app pushes a `time.sync` card"); `Sleep.h:5-6` e `Sleep.cpp:88-90` (schermata di sleep senza orologio, "No RTC on X3/X4"); `reader/ReadingStats.h:14-18` ("The device has no RTC: 'today' derives from the phone-synced ClockStore"); `BlockStatusStore.h:35-37,59-61` e `scenes/BlockScene.cpp:495-499` (countdown Block non riprendibile dopo il sonno). Quindi il lavoro non è "aggiungere un chip": è **usare un chip già montato e già interrogato**.

**NON VERIFICATO — batteria tampone (VBAT) del DS3231.** Senza VBAT il chip perde l'ora a ogni power-off e l'intero vantaggio svanisce. Il codice non dice nulla, e `probeDs3231()` passa comunque perché l'oscillatore riparte con valori BCD plausibili. Test decisivo: scrivere l'ora, staccare la batteria del device per 10 minuti, rileggere OSF (`0x0F` bit7).

### 1.2 Bus, pin, frequenza, e chi inizializza `Wire`

**FATTO — un solo bus I²C fisico, gauge + RTC + IMU, 400 kHz.** `XteinkDetect.cpp:12-14`: `SDA = 20`, `SCL = 0`, `400000`. `BoardConfig.h:599`, profilo `XTEINK_X3` campo `batteryGauge`: `{20, 0, 400000, 0x55, 0}` (BQ27220 a `0x55`, nessun charger IC). ESP32-C3 ha un solo controller: `BatteryMonitor.cpp:26-33` e `Rtc.cpp:23-27` scelgono `Wire1` solo `#if SOC_I2C_NUM > 1`, quindi qui è sempre `Wire`.

**FATTO — `Wire.begin()` è chiamato da quattro posti indipendenti**, tutti con gli stessi parametri (la `TwoWire::begin` di arduino-esp32 è idempotente, e i commenti lo dichiarano esplicitamente): `XteinkDetect.cpp:107` (poi `Wire.end()` a `:111`, pin a `INPUT` a `:112-113`); `BatteryMonitor.cpp:36-41` (`ensureWire()`, flag `g_wireReady`); `xphone-os/src/BatteryGauge.cpp:76-81` (`busAddr()`, per-operazione, commento `:75-76`); `xphone-os/src/Sleep.cpp:172-176` (`imuSleep()`, commento `:173-174`).

**Conseguenza pratica:** un driver DS3231 non deve possedere il bus, basta che segua lo stesso schema idempotente. Nessun conflitto di inizializzazione. **[INFERENZA]** l'unico vincolo è temporale: `XteinkDetect.cpp:111` fa `Wire.end()`, quindi ogni lettura RTC va fatta *dopo* `selectXteinkDevice()` (`main.cpp:166`).

Igiene: `BoardConfig.h:592` assegna `usbDetect = 20` nel profilo X3 — lo stesso GPIO di SDA. Il campo è dichiarato a `BoardConfig.h:451` e **non è letto da nessun consumer** (grep sul repo: solo la dichiarazione e il commento LilyGo a `:826`), quindi il conflitto è inerte. Rimuoverlo durante il fork, non prima.

### 1.3 La lib `Rtc` del SDK NON è riusabile così com'è

**FATTO — `freeink-sdk/libs/hardware/Rtc/` parla solo PCF8563** (`Rtc.h:3`, `Rtc.cpp:13`), e ogni differenza è una rottura silenziosa:

- `Rtc.cpp:15` — `REG_TIME = 0x02`; sul DS3231 l'ora comincia a `0x00`. Leggere 7 byte da `0x02` restituisce ore/data/mese/anno/allarme1 disallineati di due registri: **garbage**, senza errore I²C.
- `Rtc.cpp:18,88` — validità = `VL_FLAG = 0x80` nel registro secondi. Sul DS3231 il bit7 dei secondi è sempre 0; la validità sta in OSF, status `0x0F` bit7.
- `Rtc.cpp:16-17,78` — scrive `0x00` in `REG_CLKOUT = 0x0D`. Su un DS3231 `0x0D` è *Alarm2 day/date*: scrittura silenziosamente sbagliata.
- `Rtc.cpp:96-98` — bit secolo nei mesi con convenzione PCF8563 (bit7 set ⇒ 1900). Il DS3231 ha un bit Century in `0x05` bit7 ma con semantica di *toggle su overflow*.
- `Rtc.cpp:70-73` legge il bus da `BoardConfig::ACTIVE.sensors`. Nel profilo X3 `sensors` non è mai valorizzato: resta il default `NO_SENSORS` (`BoardConfig.h:464`, `:513`), quindi `rtcAddr == 0` e `Rtc::begin()` esce subito a `Rtc.cpp:71`.
- `FREEINK_CAP_RTC` vale `(FREEINK_DEVICE_STICKY)` (`BoardConfig.h:175-177`) ⇒ 0 nel build X3: la lib compila i corpi stub di `Rtc.cpp:122-130`. E `xphone-os/platformio.ini:83-90` non ha `Rtc` in `lib_deps`: **oggi non è nemmeno linkata**.

**Cosa manca**, in ordine di costo: (1) `SensorsConfig` valorizzato nel profilo X3 — `{20, 0, 400000, 0x68, 0, 0x6B, 0}`, campi a `BoardConfig.h:418-426`, `0x6B` è il QMI8658 (`Sleep.cpp:146`); abilita `hasRtc()` (`BoardConfig.h:961`) e `hasImu()` (`:963`). (2) Un discriminante di silicio: `SensorsConfig` non ha un campo "modello RTC", serve un enum `RtcChip { None, Pcf8563, Ds3231 }` o una capability `FREEINK_CAP_RTC_DS3231` che seleziona un secondo back-end — complessità **bassa**. (3) `-DFREEINK_CAP_RTC=1` e `Rtc=symlink://...` in `platformio.ini`.

### 1.4 Registri DS3231 necessari

Mappa dal datasheet DS3231 (Analog Devices/Maxim, `https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf` — il PDF è andato in **timeout** nei miei due tentativi di download del 2026-08-13; i bit sotto sono corroborati da `https://red.implrust.com/rtc/ds3231/registers.html` e `.../control-register.html`, consultati 2026-08-13, e coerenti con l'uso di `0x00` in `XteinkDetect.cpp:23`. **Da riconfermare sul PDF primario prima di scrivere il driver.**)

| Reg | Nome | Uso nel nostro caso |
|---|---|---|
| `0x00-0x06` | Seconds, Minutes, Hours, Day(1-7), Date, Month/Century, Year | Ora in BCD. `0x02` bit6 = 12/24h: **forzarlo a 0** (24h) in init. `0x05` bit7 = Century |
| `0x07-0x0A` | Alarm1 sec/min/hour/day-date | Sveglia al secondo. Serve solo se INT/SQW è raggiungibile (§1.8) |
| `0x0B-0x0D` | Alarm2 min/hour/day-date | Sveglia al minuto. `0x0D` è ciò che `Rtc.cpp:78` corromperebbe |
| `0x0E` | Control | `INTCN` (bit2)=1 → INT/SQW è uscita di allarme; =0 → onda quadra da `RS2/RS1` (bit4/3). `BBSQW` (bit6)=1 → uscita attiva anche su VBAT (solo con INTCN=0). `A2IE`/`A1IE` (bit1/0) abilitano gli allarmi. `EOSC` (bit7)=0 = oscillatore attivo su VBAT |
| `0x0F` | Status | `OSF` (bit7): 1 = oscillatore fermatosi ⇒ **ora non attendibile**, va azzerato a mano dopo il set. `A2F`/`A1F` (bit1/0) = flag allarme, azzeramento manuale. `EN32kHz` (bit3): spegnerlo |
| `0x10` | Aging Offset | Complemento a due, taratura fine del quarzo. Opzionale, default `0x00` |
| `0x11-0x12` | Temp MSB/LSB | 10 bit complemento a due, 0,25 °C. Regalo gratuito: temperatura per la dashboard senza aggiungere un sensore |

### 1.5 Sequenza di init proposta

1. Dopo `main.cpp:166` (`selectXteinkDevice()`), che chiude il bus a `XteinkDetect.cpp:111`.
2. `ensureWire()` sui pin di `ACTIVE.sensors` (§1.3 punto 1), stesso schema di `Rtc.cpp:30-42`.
3. Leggere `0x0F`. Se `OSF == 1` l'ora è spazzatura → `rtcValid = false` e si resta nel comportamento attuale (attesa di `time.sync`).
4. Scrivere `0x0E` = `0x04`: `EOSC=0`, `BBSQW=0`, `INTCN=1`, `A1IE=0`, `A2IE=0`. Nessuna onda quadra, nessun consumo su VBAT per l'uscita.
5. Scrivere `0x0F` azzerando `EN32kHz`, `A1F`, `A2F` e — se l'ora è stata appena impostata — `OSF`.
6. Leggere `0x00-0x06`; al primo set forzare `0x02` bit6 = 0 (24h). Opzionale, una volta per boot: `0x11-0x12` (temperatura).

Complessità **bassa**: ~120 righe, gemelle di `Rtc.cpp`, zero dipendenze nuove.

### 1.6 Integrazione con `ClockStore` — il punto elegante

**FATTO.** `ClockStore` (`ClockStore.h:16-21`) ha quattro scalari: `firstConnectMs`, `firstSyncMs`, `day` (yyyymmdd), `minutesIntoDay`. È scritto **solo** dalla card BLE `time.sync` (`ble/CompanionBleService.cpp:815-818`) e da `markConnected` (`:536`).

**FATTO.** `ReadingStats::todayYmd()` (`reader/ReadingStats.cpp:96-101`) calcola la data *solo* da quei campi: `day == 0 || firstSyncMs == 0 → 0`, altrimenti `elapsedMin = (millis() - firstSyncMs)/60000` con rollover via calendario civile (`:61-92`).

**Conseguenza:** se al boot, dopo una lettura RTC valida, si scrive `CLOCK_STORE.day = yyyymmdd`, `minutesIntoDay = hour*60+minute`, `firstSyncMs = millis()`, allora **`ReadingStats.cpp` non va toccato affatto**: attribuzione giornaliera, streak e finestra 7 giorni funzionano offline. È il singolo cambiamento a più alto rapporto valore/rischio del documento. File da toccare: solo `main.cpp` (nuovo stadio dopo `:166`) e `ClockStore.h` (un `bool fromRtc` per distinguere la sorgente nella About).

Direzione inversa: `time.sync` resta l'autorità — fuso orario, DST e ora legale li conosce solo l'iPhone. In `CompanionBleService.cpp:815-818`, dopo l'aggiornamento di `CLOCK_STORE`, chiamare `Rtc::set()` quando lo scarto dal DS3231 supera ~60 s. Il telefono corregge, il chip fa da volano.

### 1.7 Chi leggerebbe l'ora e dove comparirebbe

| Sito | Riferimento | Cosa cambia |
|---|---|---|
| Status bar launcher | `scenes/LauncherScene.cpp:180-201` (`StatusBar::drawBattery` a `:185`, ritorna `battLeft`) | Slot naturale per `hh:mm`: a sinistra del pallino BLE (`:191-200`), ancorato a `battLeft` |
| Schermata di sleep | `Sleep.cpp:82-133` (`drawSleepScreen`), commento `:88-90` | Oggi wordmark statico *perché* non c'è orologio. Con RTC: ora + data sul frame dormiente (`:85-86` già disegna la lista priorità) |
| About | `scenes/AboutScene.cpp:56-68` | La riga `time.sync` diventa "ora RTC + scarto dal telefono": diagnostica reale |
| Statistiche lettura | `reader/ReadingStats.cpp:96-101` | **Nessuna modifica** (§1.6) |
| Countdown Block | `scenes/BlockScene.cpp:495-543` | Vedi sotto |

**Block, in dettaglio.** `latchEnd()` (`BlockScene.cpp:201-205`) ancora la fine a `millis()`; `remainingNowMin()` (`:210-214`) sottrae. Dopo il sonno l'ancora è persa, quindi `:499` (`haveLive`) è falso e `:533-543` mostra solo l'etichetta assoluta `card.endsAtLabel` ("until 10:30 AM", campo a `BlockStatusStore.h:43`), con il commento esplicito a `:495-498` "no RTC on X3/X4". Con un RTC valido si possono ricostruire i minuti residui dall'etichetta: il countdown live sopravvive al risveglio, senza attendere la riconnessione BLE. File: `BlockScene.cpp:495-543`, commenti da correggere in `BlockStatusStore.h:35-37,59-61`. Complessità **media** — e vedi rischio 7 sulla localizzazione.

### 1.8 Sveglia temporizzata: il pin INT/SQW è raggiungibile?

**FATTO — nel repo non esiste traccia di INT/SQW.** Grep di `DS3231|SQW|3231` su tutto `flowe-os/`: solo `XteinkDetect.{h,cpp}`, il suo `library.json` e `freeink-sdk/README.md:121-125`. Nessun GPIO associato, nessun commento, nessun profilo: **il pin non è dichiarato in nessun BoardConfig**.

**FATTO — non c'è comunque un GPIO libero capace di svegliare.** Mappa X3 completa (`BoardConfig.h:578-599`, `InputManager.h:58-60`, `Sleep.cpp:399`, `platformio.ini:39-40`):

| GPIO | Uso | Fonte |
|---|---|---|
| 0 | SCL bus sensori | `BoardConfig.h:599` |
| 1 | ADC ladder pulsanti frontali | `InputManager.h:58` |
| 2 | ADC ladder coppia superiore | `InputManager.h:59` |
| 3 | Pulsante power, attivo basso | `BoardConfig.h:588`, `InputManager.h:60` |
| 4 / 5 / 6 | EPD DC / RST / BUSY | `BoardConfig.h:585` |
| 7 | SD MISO | `BoardConfig.h:587` |
| 8 / 10 | EPD SCLK / MOSI | `BoardConfig.h:585` |
| 9 / 11 | non assegnati: strapping BOOT / VDD_SPI sul C3 | — |
| 12 | SD CS | `BoardConfig.h:587` |
| 13 | latch MOSFET batteria | `Sleep.cpp:399-404` |
| 18 / 19 | USB D-/D+ (CDC on boot) | `platformio.ini:39-40` |
| 20 | SDA bus sensori | `BoardConfig.h:599` |
| 21 | EPD CS | `BoardConfig.h:585` |

**FATTO (ESP-IDF v6.0.2, doc ufficiale ESP32-C3 letta 2026-08-13: `https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/system/sleep_modes.html`).** In deep sleep "digital GPIOs (GPIO6 ~ 21) are in a high impedance state"; solo i **RTC GPIO 0~5** sono alimentati da `VDD3P3_RTC`, e la sveglia GPIO da deep sleep funziona *unicamente* su quelli ("Only GPIOs powered by the VDD3P3_RTC power domain (RTC IOs) can be used with this API"). `Sleep.cpp:412-413` usa infatti `esp_deep_sleep_enable_gpio_wakeup(1ULL << POWER_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW)` su GPIO3, e il commento `:406-411` spiega che il C3 non ha ext0/ext1. **I sei RTC GPIO 0..5 sono tutti occupati**; gli unici pin non assegnati (9 e 11) non sono wake-capable.

**FATTO — e c'è un ostacolo peggiore: a batteria il chip si spegne del tutto.** `Sleep.cpp:394-404`: GPIO13 pilota il MOSFET di latch della batteria, portato a 0 e congelato con `gpio_hold_en()`; commento `:395-398`: *"on battery the MCU is then completely powered off and the power button hard-wires a power-up regardless of the wakeup source below"*. Confermato indirettamente da `main.cpp:127-130` (il risveglio X3 può essere `POWERON`, non `DEEPSLEEP`) e da `Sleep.h:44-48`, che sceglie NVS invece della RTC memory *proprio perché* "RTC memory did not [survive]".

**Conclusione esplicita.** `esp_sleep_enable_timer_wakeup()` **non è una via d'uscita a batteria**: il timer vive nel controller RTC, che la doc dichiara essere l'unico dominio alimentato in deep sleep, ma con il latch di GPIO13 aperto quel dominio non è alimentato affatto. Il timer interno funzionerebbe **solo su USB**, e solo rinunciando al latch, cioè cambiando il modello di potenza a due stati di `Sleep.cpp:91-92`. Il codice **non contiene alcuna cifra di consumo**: le uniche affermazioni energetiche sono qualitative — `Sleep.cpp:83-84` ("the panel then holds this image at ~0 current") e `Sleep.h:27-34` (finestra di auto-sleep ridotta a 2 min durante un Block per non prosciugare la cella). Ogni stima di costo di una sveglia periodica va misurata, non dedotta.

**[INFERENZA] — l'unica strada plausibile è hardware.** INT/SQW del DS3231 è open-drain attivo-basso, e GPIO3 (power) è attivo-basso con pull-up (`PowerManager.cpp:42`, `Sleep.cpp:382`): sono elettricamente **wire-OR-abili**. Un allarme del DS3231 apparirebbe al circuito di latch esattamente come una pressione del tasto power, che `Sleep.cpp:396-398` dichiara essere un power-up hard-wired — dando sveglia temporizzata *anche a batteria*. Richiede ispezione della PCB (INT/SQW arriva a un test point?), una saldatura, e l'accettazione che il device si accenda da solo. Complessità **alta**, fuori dal firmware.

### 1.9 Checklist implementativa — RTC

1. **Misurare VBAT.** Set ora → power-off 10 min → rileggere `0x0F` bit7 (OSF). Se OSF=1, fermarsi: senza tampone il progetto non ha senso. *(nessun file)*
2. `BoardConfig.h:578-599` — valorizzare `sensors` in `XTEINK_X3`: `{20, 0, 400000, 0x68, 0, 0x6B, 0}`; sostituire `usbDetect = 20` (`:592`) con `PIN_UNASSIGNED`.
3. `BoardConfig.h:175-177` — aggiungere `FREEINK_CAP_RTC_DS3231` (default: device X3).
4. `Rtc/src/Rtc.cpp` — back-end DS3231 accanto a quello PCF8563: registri §1.4, sequenza §1.5. Validità da OSF `0x0F` bit7 e non da `VL_FLAG` (`:18,88`); `REG_TIME` da `0x02` a `0x00`; **nessuna scrittura su `0x0D`** (`:78`).
5. `xphone-os/platformio.ini:83-90` — `Rtc=symlink://../freeink-sdk/libs/hardware/Rtc` in `lib_deps` e `-DFREEINK_CAP_RTC=1` nei `build_flags`.
6. `ClockStore.h:16-21` — aggiungere `bool fromRtc = false`; riscrivere il commento `:3-8`.
7. `main.cpp` — nuovo stadio subito dopo `:166-170`: `Rtc::begin()`, `now()`, e se valido seed di `CLOCK_STORE.day/minutesIntoDay/firstSyncMs` (§1.6). **Non toccare `ReadingStats.cpp`.**
8. `ble/CompanionBleService.cpp:815-818` — dopo l'aggiornamento di `CLOCK_STORE`, `Rtc::set()` se lo scarto supera 60 s.
9. `scenes/LauncherScene.cpp:180-201` — `hh:mm` in status bar, ancorato a `battLeft`.
10. `Sleep.cpp:82-133` — ora + data sulla schermata di sleep; correggere i commenti `:88-90` e `Sleep.h:5-6`.
11. `scenes/AboutScene.cpp:56-68` — riga diagnostica RTC vs `time.sync`.
12. `scenes/BlockScene.cpp:495-543` — countdown ricostruito da `endsAtLabel` + ora RTC; correggere `BlockStatusStore.h:35-37,59-61`.
13. Opzionale, costo quasi nullo: temperatura da `0x11-0x12` per la dashboard.
14. Solo se §1.8 (INT/SQW) risulta cablabile: allarme `0x07-0x0A`, `INTCN=1`, `A1IE=1`, azzeramento di `A1F` al boot.

---

## 2. Droppare il supporto X4

### 2.1 Inventario completo dei punti X4-dipendenti

| # | Sito | Cosa si rimuove / semplifica |
|---|---|---|
| 1 | `platformio.ini:99-100,114-115,123-124` | `-DFREEINK_DEVICE_X4=1` da tutti e tre gli env. Gli env `x4` (`:119`) e `xteink` (`:110`) sono alias dichiarati identici a `x3` (`:93-94`, `:117-118`): collassano in **un solo env**. Riscrivere l'intestazione `:1-8` e il commento `:41` |
| 2-3 | `BoardConfig.h:31-33`, `:57-58`, `:66` | Normalizzazione `FREEINK_DEVICE_X4`; coerenza MCU (`FREEINK_MCU_C3` diventa `FREEINK_DEVICE_X3`) |
| 4 | `BoardConfig.h:79-83` | `FREEINK_DRIVER_SSD1677` perde il termine X4 (resta per DELINK/STICKY) |
| 5 | `BoardConfig.h:153-160` | Il commento `:156-157` ("required because X3 (gauge) and X4 (ADC) share one C3 binary") decade; `FREEINK_BATTERY_I2C_GAUGE` diventa incondizionato e `-DFREEINK_BATTERY_I2C_GAUGE=1` (`platformio.ini:103`) è ridondante |
| 6 | `BoardConfig.h:239-248` | `Board::XteinkX4` dall'enum |
| 7-8 | `BoardConfig.h:550-571`, `:574-577` | Profilo `XTEINK_X4` intero (22 righe) + i commenti "same board/pinout as X4… selected at runtime" |
| 9 | `BoardConfig.h:864-871` | Termine X4 in `MAX_FRAMEBUFFER_BYTES` |
| 10 | `BoardConfig.h:888-893` | `DEFAULT_DEVICE`: sparisce il ramo "boot as X4, runtime-swap" (`:891-892`), resta `:889` |
| 11 | `BoardConfig.h:905-909` | `case Board::XteinkX4` in `selectDevice()` |
| 12 | `BoardConfig.h:13-16,236-238` | Commenti d'architettura sul dual-profile |
| 13-14 | `XteinkDetect/` **intero** (`src/XteinkDetect.cpp` 135 righe, `include/XteinkDetect.h`, `library.json`) + `platformio.ini:89` | Cancellabile e via da `lib_deps` — ma vedi §2.3 |
| 15 | `xphone-os/src/DeviceKind.h` (9 righe) + `main.cpp:106,108` | File intero + variabile `gDeviceIsX3` |
| 16-17 | `main.cpp:161-170`, `:190` | Stadio 2 di detection (`display.setDisplayX3()` diventa superfluo) e il log condizionale `detect=X3/X4` |
| 18 | `FreeInkDisplay.h:33-34,133-134`; `FreeInkDisplay.cpp:65-75,100-107` | `setDisplayX3()` e l'enum `PanelSel`; con un solo driver linkato il `switch` di `:90-107` collassa. `X3_DISPLAY_*` (`.h:51-54`) diventano *le* costanti; `DISPLAY_WIDTH=800`/`HEIGHT=480` (`.h:47-48`) sono valori X4 e vanno corretti |
| 19 | `driver/Ssd1677Driver.{h,cpp}` (108 + 524 righe), `lut/Ssd1677Luts.h`; `FreeInkDisplay.cpp:23-24,105-106` | Non più compilati per questo target (restano nel SDK per DELINK/STICKY) |
| 20 | `xphone-os/src/Sleep.cpp:144-145,365-366` | `imuSleep()` diventa incondizionato (via `if (::gDeviceIsX3)`) |
| 21 | `xphone-os/src/BatteryGauge.cpp:258-266`, `BatteryGauge.h:1` | Ramo stub `#else !FREEINK_BATTERY_I2C_GAUGE` |
| 22 | `xphone-os/src/art/BlockArtwork.h:20-22,1848-1851,3306` + dati `:1852-3304` | Ramo X4 (`BlockHeroField` 432×297, `BlockHeroWork` 300×300): **1.459 righe di sorgente**. Bonus: `:20` usa `#if defined(FREEINK_DEVICE_X3)` e il file include solo `<cstdint>` (`:2`), quindi **non** vede la normalizzazione di `BoardConfig.h:34-36`: con `-DFREEINK_DEVICE_X3=0` selezionerebbe comunque il ramo X3. Bug latente che sparisce |
| 23 | `ble/CompanionProtocol.h:25` | `deviceName()` diventa la costante `"xphone X3"` |
| 24 | `net/FileTransferServer.cpp:154-157` | `extern bool gDeviceIsX3` + campo JSON `device` |
| 25 | `scenes/SettingsScene.cpp:300-301` | Footer `(x3/x4)` → `(x3)` |
| 26 | `.github/workflows/firmware-release.yml:57,62-65,87-93` | Build di due env, asset `flowe-x4*.bin`, checksum |
| 27 | `README.md:106-110`; `xphone-os/README.md:3-6,58-60,194-197` | Istruzioni e disclaimer X4 |
| 28 | Commenti "X3/X4" sparsi | `Sleep.h:5`, `Sleep.cpp:89,145`, `Gfx.h:16-19,116`, `Gfx.cpp:8-9`, `Input.h:6,103`, `Scene.cpp:26`, `BlockStatusStore.h:60`, `BlockScene.cpp:497`, `SdUpdate.cpp:548`, `ReadingStats.h:15` |

**Semplificazioni strutturali, di valore superiore al risparmio di byte.** `Gfx.cpp:8-11` legge la geometria a runtime e `Scene.cpp:25-28` calcola i margini in percentuale perché deve servire 528×792 *e* 480×800: con un solo pannello la geometria logica diventa costante di compilazione. `InputManager.h:60` (`POWER_BUTTON_PIN = DEFAULT_DEVICE.input.power`) smette di dipendere dal "device di default conservativo": oggi `DEFAULT_DEVICE` è `XTEINK_X4` (`BoardConfig.h:891-892`) e il valore è corretto solo perché i due profili condividono `power = 3`.

### 2.2 Guadagni RAM/flash, con i calcoli

`panelBytes(p) = (p.displayWidth / 8) * p.displayHeight` (`BoardConfig.h:861-863`). X3: `792/8 = 99`, `99 × 528 = **52.272 B**`. X4: `800/8 = 100`, `100 × 480 = **48.000 B**`. Quindi `MAX_FRAMEBUFFER_BYTES = cmax(48.000, 52.272) = **52.272**` (`BoardConfig.h:864-871`).

| Voce | Prima | Dopo | Guadagno | Calcolo / fonte |
|---|---|---|---|---|
| Framebuffer statico (.bss) | 52.272 B | 52.272 B | **0 B** | Un solo array `frameBuffer0[MAX_BUFFER_SIZE]` (`FreeInkDisplay.h:148`) perché `EINK_DISPLAY_SINGLE_BUFFER_MODE=1` (`platformio.ini:42`) esclude `frameBuffer1` (`.h:151-156`). L'X3 **è** il pannello più grande: `cmax(0, 52.272) = 52.272`. Istanza unica a `main.cpp:51` |
| LUT SSD1677 (.rodata) | 456 B | 0 | **456 B** | `lut/Ssd1677Luts.h`: `lut_grayscale` 116 + `lut_grayscale_revert` 116 + `lut_factory_fast` 114 + `lut_factory_quality` 110 = 456 (conteggio dei letterali `0xNN`) |
| Codice `Ssd1677Driver` (.text) | 524 righe .cpp + 108 .h | 0 | **[INFERENZA] ~4-6 KB** | Nessuna build eseguita; ordine di grandezza per un driver EPD con `-Os` + `-flto` (`platformio.ini:63`) |
| Singleton `Ssd1677Driver` (.bss) | ~32 B | 0 | **~32 B** | `Ssd1677Driver.h:83-102`: 1 ref + 3×u16 + 1×u32 + 6 bool + vptr; Meyers singleton `:105-106` |
| `ssd1677DefaultConfig()` (.rodata) | ~16 B | 0 | **~16 B** | `Ssd1677Driver.h:19-44` (`booster[5]` + 7 scalari + padding) |
| `XteinkDetect` (.text) | 135 righe | 0 | **[INFERENZA] ~0,6-1 KB** | Nessun guadagno di libreria: `Wire` resta usata da `BatteryMonitor.cpp:15`, `BatteryGauge.cpp:36`, `Sleep.cpp:10` |
| Copia rodata di `XTEINK_X4` | 1 × `sizeof(BoardProfile)` | 0 | **[INFERENZA] ~120-160 B** | `constexpr BoardProfile XTEINK_X4` (`BoardConfig.h:551-571`) materializzata nelle TU che chiamano `selectDevice()` (`:905-908`) |
| Artwork Block ramo X4 | **0 B nel binario** | 0 | **0 B flash** | `BlockHeroField` 432×297: `54 × 297 = 16.038` B; `BlockHeroWork` 300×300: `⌈300/8⌉ = 38`, `38 × 300 = 11.400` B; totale **27.438 B** — ma già escluso a compile-time da `#if defined(FREEINK_DEVICE_X3)` (`BlockArtwork.h:20,1848`). Guadagno reale: **−1.459 righe di sorgente** |
| Artwork X3, per confronto | 19.800 + 14.620 = 34.420 B | idem | 0 | `60 × 330 = 19.800`; `⌈340/8⌉ = 43`, `43 × 340 = 14.620`. Conteggio dei letterali confermato |
| Asset di release | 2 × `firmware.bin` | 1 | **~50%** | `firmware-release.yml:57,62-65` |

**Totale onesto: ~5-7 KB di flash, ~50 B di RAM, zero byte di framebuffer.** Il guadagno vero del fork non è la memoria: è **eliminare un asse di variabilità** — un profilo, un driver, una geometria, un env, un binario — e con esso 1.459 righe di artwork morto, la detection I²C al boot (che oggi apre e chiude il bus prima di qualsiasi altra cosa, `XteinkDetect.cpp:107-113`) e i ~28 siti condizionali della tabella §2.1.

### 2.3 Detection minimale di sicurezza — decisione consigliata

Il rischio è documentato in prima persona nel repo, non ipotetico. `platformio.ini:6-8`: *"per-device bins bricked testers in the field — both release zips ship a file named update.bin, and the wrong one inits the wrong panel controller (frozen screen, dead-looking buttons)"*; `DeviceKind.h:6-8` e `main.cpp:162-163` ripetono la stessa cosa. `firmware-release.yml:67,72` genera `update.bin` e `flowe-x3/update.bin` dal build X3 e `:63` produce `flowe-x4.bin`: le release passate contengono entrambi. E `SdUpdate.cpp:156-160` valida magic, tabella dei segmenti, checksum XOR e SHA256 — **un'immagine X3 su un X4 passa tutti i controlli**, perché è un binario ESP32-C3 perfettamente valido. Nessuna barriera.

**Consiglio: tenere una detection minimale, non cancellarla.** Non l'intero `XteinkDetect` (due passate, tre probe, bus-clear a nove impulsi `:82-104`), ma una sola funzione: probe del BQ27220 a `0x55` (`XteinkDetect.cpp:52-58`, che già valida SoC ≤ 100 e 2500-5000 mV, non un semplice ACK) più il bus-clear, prima di `display.begin()` in `main.cpp:195`. Se assente: non inizializzare il pannello, scrivere su seriale e su `/boot-trace.txt` (infrastruttura già pronta, `main.cpp:110-125`), poi `esp_deep_sleep_start()`. Costo ~40 righe, **[INFERENZA]** ~300 B. Beneficio: un X4 che riceve la nostra `update.bin` resta ri-flashabile invece di sembrare un mattone.

### 2.4 Checklist implementativa — solo X3

1. `platformio.ini` — un solo `[env:x3]`: via `-DFREEINK_DEVICE_X4=1` (`:99-100`), via gli env `xteink` (`:110-115`) e `x4` (`:119-124`), via `XteinkDetect` da `lib_deps` (`:89`); riscrivere `:1-8` e `:41`.
2. `BoardConfig.h`, nell'ordine: `:31-33`, `:57-58`, `:66`, `:79-83`, `:239-248`, `:550-571`, `:864-871`, `:888-893`, `:905-909`; poi i commenti `:13-16`, `:236-238`, `:153-160`, `:574-577`.
3. `FreeInkDisplay.{h,cpp}` — rimuovere `PanelSel`/`setDisplayX3()` (`.h:33-34,133-134`; `.cpp:65-75,100-107`); portare `DISPLAY_WIDTH/HEIGHT` (`.h:47-50`) a 792×528 e ritirare `X3_DISPLAY_*` (`.h:51-54`).
4. **Prima di cancellare `XteinkDetect`**: estrarre la probe minimale (§2.3) e cablarla in `main.cpp` prima di `:195`. Solo dopo eliminare la lib e `DeviceKind.h`.
5. `main.cpp` — `:106,108` (`gDeviceIsX3`), `:161-170` (stadio 2), `:190` (log).
6. Consumer di `gDeviceIsX3`: `Sleep.cpp:365-366`, `CompanionProtocol.h:25`, `FileTransferServer.cpp:154-157`, `SettingsScene.cpp:300-301`.
7. `art/BlockArtwork.h` — eliminare `:1848-3304` e rendere incondizionato `:20-1846`; aggiornare il commento `:10-11`.
8. `BatteryGauge.cpp:258-266` — via lo stub ADC; `BatteryGauge.h:1`; `platformio.ini:101-103`.
9. `.github/workflows/firmware-release.yml:57,62-65,87-93` — un solo env, un solo `.bin`, checksum ridotti.
10. `README.md:106-110`; `xphone-os/README.md:3-6,58-60,194-197`.
11. Pulizia dei commenti "X3/X4" (riga 28 della tabella §2.1) — ultima, meccanica, senza rischio.
12. Solo con i punti 1-11 verdi: valutare la geometria costante in `Gfx.cpp:8-11` e `Scene.cpp:25-28`. **È un cambiamento di layout, non di build: va fatto a parte.**

---

## Rischi

1. **VBAT del DS3231 sconosciuto (§1.1).** Senza tampone, o con tampone scarico/assente, il chip perde l'ora a ogni power-off e tutta la §1 diventa inutile. `probeDs3231()` (`XteinkDetect.cpp:60-66`) non lo rivela: valida solo che i secondi siano BCD plausibili. *Mitigazione: il test 1 della checklist §1.9 precede ogni riga di codice.*
2. **Riuso ingenuo della lib `Rtc` = ora sbagliata, silenziosamente.** `Rtc.cpp:15` legge da `0x02` e `:78` scrive in `0x0D`: sul DS3231 la prima restituisce campi disallineati di due registri, la seconda corrompe Alarm2. Nessun errore I²C — tutti gli ACK arrivano. *Mitigazione: dispatch per silicio (§1.3 punto 2), mai un `#define` di indirizzo riadattato.*
3. **Sveglia temporizzata: non esiste, e il software non basta.** Tutti e sei i RTC GPIO (0-5) del C3 sono occupati (§1.8), INT/SQW non è dichiarato in nessun profilo, e il latch di GPIO13 (`Sleep.cpp:394-404`) spegne il chip *completamente* a batteria — quindi nemmeno `esp_sleep_enable_timer_wakeup()` funziona lì. Promettere "il device si sveglia alle 7:00" senza aver prima ispezionato la PCB è un impegno che il firmware non può mantenere.
4. **Perdere la detection rende un X4 apparentemente brickato (§2.3).** `SdUpdate.cpp:156-160` valida l'integrità, non il target: la nostra `update.bin` viene accettata da un X4 e ne congela il pannello. Il repo dichiara che è già capitato (`platformio.ini:6-8`). *Mitigazione: probe minimale con rifiuto di avvio, ~40 righe.*
5. **Aspettarsi memoria dal drop di X4 è un errore di premessa.** L'X3 è il pannello *più grande* (52.272 > 48.000 B): il framebuffer non si muove di un byte, e l'artwork X4 non è nel binario. Il guadagno reale è ~5-7 KB di flash più la sparizione di un asse di variabilità. Pianificare su "recuperiamo 4 KB di RAM" porterebbe a decisioni sbagliate altrove.
6. **`ClockStore` è scritto da due task.** Oggi `firstConnectMs` arriva dal task NimBLE (`CompanionBleService.cpp:536`) e i campi data dal main loop (`:815-818`); il commento `ClockStore.h:10-12` giustifica l'assenza di mutex con l'atomicità degli scalari 32/16 bit sul C3. Una terza sorgente (RTC al boot, §1.6) è sicura *solo* se resta un seed di boot prima dell'avvio della radio: un refresh periodico dell'RTC dal loop romperebbe l'invariante `firstSyncMs` su cui `ReadingStats.cpp:96-101` calcola i minuti trascorsi.
7. **Parsing di `endsAtLabel` e localizzazione italiana.** `BlockScene.cpp:539-540` formatta `"until %s"` da una stringa prodotta dall'iPhone (`BlockStatusStore.h:43`, 16 byte). Ricostruire il countdown (§1.7) significa fare parsing di quel testo: con locale `it-IT` diventa "10:30" invece di "10:30 AM", e un parser scritto sull'inglese fallirebbe *proprio* nella versione localizzata. Il campo va reso numerico nel protocollo, non interpretato.
8. **Ordine dei lavori.** §2 tocca `BoardConfig.h:550-599` e §1 tocca lo stesso profilo `XTEINK_X3`. Farli in parallelo produce conflitti dentro lo stesso letterale di inizializzazione **posizionale**, dove un campo fuori posto compila senza errori e assegna il pin sbagliato. *Prima §2 (meccanico, verificabile), poi §1.*
