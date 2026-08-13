# Roadmap versionata del fork

Sostituisce la scaletta preliminare: incorpora le decisioni prese (vedi
[14-decisioni.md](14-decisioni.md)) e i risultati di fattibilità verificati in
[12-rtc-e-solo-x3.md](12-rtc-e-solo-x3.md) e
[13-personalizzazione-e-i18n.md](13-personalizzazione-e-i18n.md).

Ogni versione è **rilasciabile**: fa una cosa in più e resta usabile. Nessuna stima
in giorni; solo complessità e prova di funzionamento.

## Sintesi

| Ver. | Titolo | Contenuto | Blocchi esterni |
|---|---|---|---|
| v0.1 | Baseline | fork, build, flash, rebranding, solo-X3 | — |
| v0.2 | App iOS minima | BLE central + bonding + `time.sync` + Priorities | — (account gratuito basta) |
| v0.3 | Italiano | i18n firmware con switcher + app localizzata + formati IT | — |
| v0.4 | Orologio vero | driver DS3231, ora offline, countdown reali | — |
| v0.5 | Dashboard | modalità scrivania configurabile | v0.4 |
| v0.6 | Today + notifiche | EventKit, meteo °C, filtro ANCS | — |
| v0.7 | Libri via Wi-Fi | `reader.shelf` + `transfer.wifi` + upload HTTP | — |
| v0.8 | Block | Screen Time reale | **ADP 99 $/anno** |
| v0.9 | Voce e AI in italiano | `SpeechTranscriber it-IT` + FoundationModels | iPhone 15 Pro+ per l'AI |
| v1.0 | Personalizzazione | icone/sleep/blocco/spinner/font | tool asset da riscrivere |
| v1.1+ | Backlog | IMU, orizzontale, nuove app, igiene | — |

---

## v0.1 — Baseline "è mio e compila"
**Stato:** completata e accettata su X3 da Maurizio il 13/08/2026.


1. Fork privato + `upstream` remoto; branch `main` tuo. Upstream pubblica solo commit
   squash "Sync from flowe monorepo @ …": si segue con `git diff <sync_prec>..<sync_nuovo>`,
   non con rebase.
2. `../.venv/bin/pio run -e lume-x3` a freddo, dimensioni annotate come baseline.
3. Flash USB con il cavo pogo 4 pin + monitor a 115200.
4. **Rebranding**: nome BLE `Lume X3`, versione `0.1.0-dev`, wordmark/mark
   geometrici, hostname mDNS `lume`, artefatti release `lume-x3`.
5. **Solo X3** (complessità bassa, guadagno = semplicità non RAM: il framebuffer resta
   52.272 B perché l'X3 è il pannello *più grande*): un solo env, via il driver SSD1677
   (−456 B di LUT, ~5-7 KB di `.text` stimati), via i 1.459 righe di artwork X4 morto,
   via i 28 siti condizionali. **Da tenere**: un probe minimale del BQ27220 prima di
   `display.begin()` che rifiuta l'avvio su hardware non-X3 (~40 righe) — senza questo
   un tuo `update.bin` su un X4 lo fa sembrare brickato.

*Prova*: device che boota col tuo wordmark e la tua versione in About.

## v0.2 — App iOS minima (il vero inizio)

Spec: [03-protocollo-ble.md](03-protocollo-ble.md). Con **account Apple gratuito**:
firma valida 7 giorni, `EventKit` e `bluetooth-central` funzionano, Screen Time no.

App:
1. [x] `CBCentralManager`, scan **per `SERVICE_UUID`** Lume (`F39F34A5-…-6BAE`),
   connessione/restoration e subscribe Action Notify prima di scrivere.
2. [x] `Info.plist`: `NSBluetoothAlwaysUsageDescription`,
   `UIBackgroundModes = [bluetooth-central]` e restore identifier.
3. [x] `time.sync` subito dopo la connessione (`day` = `yyyymmdd`,
   `minutesIntoDay`).
4. [x] Priorities locale + `priorities.snapshot` (≤10 item, ≤512 B,
   split `part`/`parts`) + consumo di `priority.toggle`.
5. [x] Prova end-to-end su iPhone 16 Pro/iOS 27: pairing, `time.sync`,
   snapshot Priorities, toggle X3→iPhone e reconnect dopo reinstallazione.
6. [x] Today/EventKit: consenso Calendario + Promemoria, proiezione delle
   prossime 24 ore, snapshot `today.snapshot` entro 512 B, refresh manuale e
   risposta a `today.sync.request`; verificato sul vero iPhone/X3.

Firmware:
* [x] Gate GATT cifrato per Card Write e Action Read/Notify.
* [x] Resync ANCS sticky durante reconnect/discovery, con retry di subscribe.
* [x] Timeout app di 8 secondi sulle write, con reconnect automatico.
* [x] Prova hardware del firmware hardenizzato e identità BLE Lume isolata
  dall'app upstream ancora installata.

*Prova di accettazione*: priorità create sul telefono che compaiono sul vetro e
si spuntano col tasto; agenda reale che appare in Today; reconnect senza riavviare
l'app. Tutti e tre i percorsi sono verificati su hardware.

## v0.3 — Italiano completo a compile time
**Stato:** implementazione e build completate; verifica visiva su X3 pendente.

Decisione aggiornata: niente switcher runtime. Il firmware produce due immagini,
`lume-x3-it` (default) e `lume-x3-en`; `L10N()` seleziona un solo letterale nel
preprocessore, quindi la lingua esclusa non occupa flash/RAM e non esistono stato
NVS o cambio a caldo.

Completato:
* tutte le scene, splash, sleep e soft-key localizzati;
* token BLE/JSON ed euristiche (`reminder`, `All day`, stati Block, suffissi
  transfer) lasciati canonici;
* `TODAY/TONIGHT/TOMORROW` tradotti soltanto al render;
* plurali variadici corretti senza cambiare la lista argomenti;
* workflow release con `update_it.bin`, `update_en.bin`, due zip e manifest per
  locale;
* build 2/2 `SUCCESS`, immagini ESP32-C3 valide e ricerca byte che prova
  l'assenza della lingua non selezionata.

*Prova residua*: flashare `lume-x3-it` e controllare sul vetro tab, righe lunghe,
stati vuoti e glifi accentati. Dettagli in
[13-personalizzazione-e-i18n.md](13-personalizzazione-e-i18n.md).

## v0.4 — Orologio vero (DS3231)

L'X3 ha un **DS3231 montato** (più un IMU QMI8658), oggi interrogati solo come
fingerprint di identità; l'X4 non li ha, da qui il "no RTC" del codice.

* La lib `Rtc` del SDK parla **solo PCF8563** e non è nemmeno in `lib_deps`: usarla
  produrrebbe letture disallineate di 2 registri **senza errori I2C**. Serve un driver
  DS3231 nuovo (I²C 0x68 su SDA20/SCL0, lo stesso bus del gauge): tempo BCD da 0x00,
  flag OSF in 0x0F.
* Integrazione a costo quasi nullo: `ReadingStats::todayYmd()` usa solo `day`,
  `minutesIntoDay`, `firstSyncMs` di `ClockStore` → seedandoli al boot dall'RTC,
  statistiche, streak e countdown Block funzionano **offline** senza toccare altro.
* **Limite verificato**: la sveglia temporizzata da deep sleep non è ottenibile in
  software. I soli GPIO wake-capable dell'ESP32-C3 (0-5) sono tutti occupati, e a
  batteria il latch GPIO13 toglie corrente all'MCU. Richiederebbe un wire-OR del pin
  INT/SQW su GPIO3 = saldatura. Da valutare solo se la dashboard "sempre aggiornata"
  diventa un requisito duro.

*Prova*: device scollegato dal telefono per un giorno che mostra l'ora giusta al risveglio.

## v0.5 — Dashboard da scrivania

Il pezzo più "tuo". Oggi esistono solo: poster di sleep (3 varianti fisse),
auto-sleep a 10 min, refresh su evento.

* Modalità dashboard: restare sveglio con repaint a bassa frequenza (o wake periodico),
  scegliere **cosa** mostrare (priorità, prossimo evento, ora, batteria, statistiche
  lettura) e con quale layout.
* Vincoli reali da rispettare: ghosting → uno scrub HALF ogni N refresh (già previsto
  dal driver); durata pannello; consumo (il codice non contiene cifre, quindi va misurato).
* Punti da modificare: `main.cpp` (auto-sleep e pump), `Sleep.cpp::drawSleepScreen`,
  nuova scena "Dashboard" + voce in Settings.

*Prova*: device in piedi sulla scrivania che mostra un quadro utile per un'intera
giornata, con consumo misurato.

## v0.6 — Today e notifiche

* App: EventKit (`NSCalendarsFullAccessUsageDescription`, `NSRemindersFullAccessUsageDescription`,
  `requestFullAccessToEvents/Reminders`) → `today.snapshot` (≤6 item) in risposta a
  `today.sync.request`, che **l'app originale non implementa** — quindi oggi la soft-key
  SYNC di Today non fa nulla: è un requisito, non un bug.
* Meteo: **Open-Meteo** (WeatherKit richiede ADP; Open-Meteo free è non-commerciale,
  ≤10.000 chiamate/giorno, CC BY 4.0 → attribuzione visibile). Stringhe già in °C.
* Notifiche: `notif.apps.request` → picker, `notif.filter` (≤16 bundle id, ≤31 char).

*Prova*: agenda di domani sul vetro, con banda meteo in °C, senza aprire l'app.

## v0.7 — Libri via Wi-Fi

* `reader.shelf.request` → inventario chunkato (`tok`/`seq`/`done`, ≤128 libri).
* `transfer.wifi` (SSID+password) → `transfer.start` → il device sale in Wi-Fi (il BLE
  si spegne fino al reboot: è un vincolo di RAM, non un bug) e risponde `transfer.status`
  con l'IP; l'app carica gli EPUB via HTTP (`POST /upload`).
* UI libreria con i tre stati visti negli screenshot (on device / phone only / device only).
* Nota: la modalità è effimera e volontaria, quindi l'assenza di autenticazione HTTP è
  accettabile per ora. Se un giorno la lasci accesa a lungo: token mostrato a schermo.

*Prova*: EPUB scelto sull'iPhone che appare nella griglia del reader.

## v0.8 — Block con Screen Time (richiede account a pagamento)

**Verificato sulla documentazione Apple**: `com.apple.developer.family-controls` **non
è disponibile** con account gratuito/Personal Team (quindi nemmeno via
AltStore/SideStore), è disponibile *subito* in sviluppo con ADP a 99 $/anno, e richiede
una **richiesta separata ad Apple** solo per distribuire (TestFlight/App Store). I
marketplace alternativi UE e la Web Distribution non aggirano la membership.

* `AuthorizationCenter.requestAuthorization(for: .individual)`, `FamilyActivityPicker`,
  `ManagedSettingsStore` per gli shield, `DeviceActivity` per la fine del blocco.
* Card `block-status` con `state` sempre esplicito + `blocksToday`/`blockStreak`/`blocksTotal`
  calcolati sul telefono; risposta entro **4000 ms** ai comandi `block.start/break/stop`,
  altrimenti il device passa in stato ottimistico.
* Attenzione all'issue upstream #32 (shield che non si riapplica dopo la pausa se l'app
  non viene riaperta): la logica di riapplicazione va nel `DeviceActivityMonitor`, non
  nel foreground.

*Prova*: app social effettivamente bloccate con countdown sul device.

## v0.9 — Voce e AI in italiano

Verificato: l'italiano è supportato sia da Apple Intelligence (di sistema da iOS 18.4)
sia dal **Foundation Models framework** (iOS 26, italiano nel set di lancio), e da
`SpeechTranscriber` (Italian, 2 regioni). Nessun costo, nessun entitlement per il
modello base; serve iPhone 15 Pro/16+ con Apple Intelligence attiva (~7 GB).

Architettura consigliata (evita il difetto attuale dell'app originale, che capisce solo
l'inglese):
1. `SpeechAnalyzer` + `SpeechTranscriber.supportedLocale(equivalentTo: it-IT)` con
   `AssetInventory` per il download del modello; permesso `NSMicrophoneUsageDescription`
   (con `SpeechAnalyzer` **non** serve `NSSpeechRecognitionUsageDescription`).
2. Parser deterministico prima dell'AI per i pattern certi (`5x5`, `3 km`, orari, date)
   con `NLTagger`/regex e lessico italiano.
3. `LanguageModelSession` one-shot (context 4096 token) con `@Generable`, istruzioni
   `The person's locale is it_IT.` + `You MUST respond in Italian.`, e
   `SystemLanguageModel.availability` + `supportsLocale` controllati a runtime.
4. Fallback esplicito su editor di conferma quando l'AI non c'è; cloud solo opt-in con
   chiave dell'utente nel Keychain.

*Prova*: "domani palestra, panca 5x5 e chiamare il commercialista" dettato in italiano
che diventa priorità + workout corretti.

## v1.0 — Personalizzazione

Ordine per complessità crescente (dettagli e formati in [13](13-personalizzazione-e-i18n.md)):

| Feature | Complessità | Cosa serve |
|---|---|---|
| Cosa mostrare nella schermata di blocco/sleep | bassa/media | `Sleep.cpp::drawSleepScreen` (3 varianti fisse) → layout selezionabile + chiave NVS |
| Icona di caricamento / spinner | bassa | oggi non esiste uno spinner: solo frecce `SyncIndicator` e dot in status bar |
| Icone launcher tue | media | formato noto (104×104, 1 bpp MSB-first, bit 0 = ink, 1.352 B/icona, 5 pack × 6 app) ma **il generatore `build_launcher_icons.py` non è nella repo**: va riscritto |
| Artwork del blocco | media | hero 480×330 = 19.800 B, stesso formato; generatore assente |
| Più font per il reader | media | 2 bpp compressi a gruppi DEFLATE; una famiglia costa 227-280 KB di flash (12 header = 744 KB oggi); **`fontconvert.py` non è nella repo** → va riscritto, oppure caricare da SD (alta: il decompressore assume stream contiguo in RAM) |
| Reader in orizzontale | alta | la rotazione è in un punto solo (`Gfx::drawPixel`) ma il portrait è assunto in viewport reader, `flushWindow` (allineamento 8 px), soft-key bar, coordinate dormant, griglia cover; **la cache `section.bin` si invalida da sola** perché header contiene viewport w/h |

## v1.1+ — Backlog

* **IMU QMI8658** (montato, driver assente — quello del SDK è per LSM6DS3): auto-rotate
  per il reader orizzontale, tap per girare pagina.
* Nuove app: pomodoro, habits, flashcard, read-to-unblock (idee già in upstream #13/#14/#27).
* Igiene: rollback OTA `PENDING_VERIFY`, CI su push/PR, versione dal tag, cache CI,
  patch framework in `core_dir` locale, test nativi del reader.
* Reader: wrap dei titoli (upstream #24), immagini con segnaposto, sweep della cache
  orfana, statistiche corrette, indicizzazione a chunk.
