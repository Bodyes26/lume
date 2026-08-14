# Lume — stato operativo e handoff

**Aggiornato:** 14 agosto 2026  
**Versione firmware:** `0.1.0-dev`  
**Versione app iOS:** `0.2.0-dev`  
**Base upstream:** `andrewjiang/flowe-os@3101448b02362e627cb17c4de863c1ed22d2478d` (`fw-v0.5.0`)  
**Commit vertical slice Priorities:** `75639a3`  
**Commit Today/EventKit + icona:** `90777cf`  
**Commit orologio DS3231 (v0.4):** `b2a515e`  
**Fase:** v0.4 orologio hardware DS3231 completata, flashata e accettata sull'X3.

Questo file descrive soltanto lavoro realmente osservato. Per riprendere da una nuova
sessione, partire da [START-HERE.md](START-HERE.md).

## Stato semaforo

| Area | Stato | Evidenza / limite |
|---|---|---|
| Documentazione persistente | **VERIFICATA** | 14 capitoli + START/HANDOFF dentro `docs/lume/` |
| Toolchain locale | **VERIFICATA** | Python 3.11, `.venv`, PlatformIO Core 6.1.19 |
| Build upstream di riferimento | **VERIFICATA** | `xteink` prima delle modifiche: SUCCESS; flash 2.532.155 B, RAM 145.004 B |
| Build Lume X3 localizzate | **VERIFICATA** | IT: RAM 144.996 B, flash 2.529.849 B, bin 2.542.384 B; EN: RAM 144.996 B, flash 2.529.125 B, bin 2.541.664 B |
| Identità a compile time | **VERIFICATA** | stringhe, UUID GATT Lume, identità BLE random-static, asset/env/release compilano |
| Localizzazione firmware | **VERIFICATA SU HARDWARE** | `lume-x3-it` caricato via USB sull'X3; boot, launcher, Impostazioni, una scena applicativa, soft-key e glifi accentati confermati corretti dall'utente; `lume-x3-en` verificato a build e isolamento binario |
| Boot e resa sul vetro pre-i18n | **VERIFICATA DALL'UTENTE** | Maurizio ha provato le schermate e confermato il funzionamento complessivo prima della localizzazione |
| BLE/ANCS reale v0.1 | **VERIFICATA** | nome advertising `Lume X3`; notifica WhatsApp ricevuta e renderizzata |
| App iOS Lume | **VERIFICATA SU HARDWARE** | build firmata su iPhone 16 Pro/iOS 27; Priorities bidirezionale, Today/EventKit, reconnect e nuova icona riusciti |
| Orologio hardware DS3231 | **VERIFICATA SU HARDWARE** | `lume-x3-it` flashato sull'X3: `[lume] rtc: DS3231 2026-08-14 19:22:12`, OSF a 0, secondo boot un minuto dopo, ora coincidente col `time.sync`. Maurizio ha poi confermato vetro e tampone VBAT |

## Cosa è stato implementato in v0.1

### Fork e continuità

* L'origin upstream è stato rinominato `upstream`; fetch punta a
  `https://github.com/andrewjiang/flowe-os.git`, push è impostato a `DISABLED` per
  impedire invii accidentali. Non esiste ancora `origin`: aggiungerlo soltanto quando
  Maurizio crea il repository GitHub di Lume.
* Tutta l'analisi prima esterna in `../docs-fork/` è stata spostata e versionabile in
  `docs/lume/`. I link relativi tra capitoli restano validi.
* `.venv/` è ignorato da Git. La toolchain installata localmente è PlatformIO 6.1.19;
  CI usa Python 3.11.

### Identità Lume

* BLE advertised name: `Lume X3` in `xphone-os/src/ble/CompanionProtocol.h`.
* Servizio e caratteristiche GATT hanno UUID Lume propri; una identità BLE
  random-static deterministica, derivata dal MAC hardware, impedisce alla vecchia
  app Flowe di riusare il peripheral e gli handle CoreBluetooth in cache.
* Versione a schermo/protocollo HTTP: macro `XPHONE_VERSION` in
  `xphone-os/src/scenes/AppScenes.h`, default `0.1.0-dev` per le build locali e
  sovrascritta con la versione del tag dal workflow di release (il simbolo storico
  `XPHONE_VERSION` è mantenuto per ridurre il diff upstream; non è testo visibile).
* Boot splash: wordmark `lume` + nuovo mark geometrico scalabile in
  `xphone-os/src/art/LumeMark.h`; il bitmap `FloweLogo.h` è stato rimosso.
* Launcher, sleep face, dormant priorities, About, Settings e File Transfer mostrano
  Lume. Hostname mDNS: `lume.local`. La root HTTP dice `Lume file transfer`.
* Namespace NVS `xphone` **intenzionalmente invariato**: non è visibile e cambiarlo
  perderebbe pairing/settings/posizione reader durante gli aggiornamenti.
* Tag dei log seriali `[xphone-os]` per ora **intenzionalmente invariati** nella maggior
  parte dei moduli: sono diagnostica interna e mantenerli riduce rumore nei merge
  upstream. I nuovi messaggi del guard usano `[lume]`.

### Clean cutover X3

* Un solo target hardware X3, con due env di locale: `lume-x3-it` (default) e
  `lume-x3-en`. Entrambi definiscono `FREEINK_DEVICE_X3=1` e non
  `FREEINK_DEVICE_X4`; il compilatore elimina SSD1677 e i rami/artwork X4.
* `boot()` chiama `detectXteinkIsX3()` prima di SPI/display. Se il fingerprint non
  trova almeno 2 fra BQ27220/DS3231/QMI8658 in entrambe le passate, stampa FATAL e
  resta fermo: nessun comando viene inviato al pannello sbagliato.
* Dopo il guard, `display.setDisplayX3()` seleziona esplicitamente UC8253 e geometria
  792×528.
* Rimossi `DeviceKind.h`, `gDeviceIsX3` e tutti i rami runtime consumer: nome BLE,
  footer Settings, status HTTP e sleep IMU sono X3 deterministici.
* La FreeInk SDK vendorizzata mantiene il profilo X4 sorgente perché è una libreria
  generale; **non viene compilato** nel firmware Lume.

### Release e README

* `.github/workflows/firmware-release.yml` compila `lume-x3-it` e
  `lume-x3-en`; produce `update_it.bin`, `update_en.bin`, copie
  unversioned/versionate, due zip con `update.bin`, checksum e `latest.json`
  con `assets.x3.defaultLocale` e `assets.x3.locales.it/en`.
* `README.md` e `xphone-os/README.md` distinguono Lume dall'upstream e riportano
  i comandi reali; il README principale documenta anche il vertical slice iOS.

## App iOS v0.2 — vertical slice implementato

Il sorgente nativo SwiftUI è in `ios/` e il progetto Xcode è rigenerabile da
`ios/project.yml` con XcodeGen. Bundle id `com.maurizio.lume`, deployment target
iOS 17, stato e testi in italiano.

Comportamento implementato:

* shell visuale Lume con mark geometrico, palette carta/inchiostro e layout
  Priorità verificato su iPhone 17 Pro Simulator;
* archivio locale persistente di massimo dieci priorità; aggiunta, modifica,
  completamento, eliminazione e riordino;
* discovery per UUID Lume, reconnect dell'ultimo peripheral e CoreBluetooth
  state restoration; chiavi di restoration v2 per la migrazione alla nuova identità;
* subscribe ad Action Notify con watchdog/retry durante pairing, `time.sync`
  automatico e snapshot `priorities.snapshot` multipart entro MTU/limite firmware;
* gestione `priority.toggle` e `priorities.sync.request`;
* coda GATT sequenziale con write-with-response, timeout a 8 secondi e reconnect;
* 4 contract test host per tempo locale, schema snapshot, split ordinato e
  decode toggle.

Hardening firmware v0.2 già applicato e compilato:

* Card Write richiede link cifrato; Action Read/Notify dichiara accesso cifrato;
* la richiesta di resync ANCS resta pendente durante reconnect/discovery e
  ritenta dopo errori di subscribe, senza lasciare il CCCD spento.

Verifica osservata il 13/08/2026:

```text
Xcode 27 beta / iPhoneOS 27: BUILD SUCCEEDED con firma Apple Development
installazione e launch su iPhone 16 Pro fisico: riusciti
swift test: 4 test, 0 failure
firmware lume-x3: SUCCESS; RAM 144.996 B; flash 2.527.197 B
firmware.bin: 2.539.728 B
SHA-256: 0aa49a6e4e128cdae1b73ab69f3b4c0a9297301576a952760e05ef07c255ba7d
```

Nessun blocco esterno residuo per build/installazione: Xcode beta è in
`/Applications/Xcode-beta.app`, l'account Apple è registrato, il provisioning
automatico usa il team `XTU68E98BM` e l'app firmata è installata sul telefono.

## Evidenza di build localizzata

Comando eseguito dalla root modulo:

```sh
cd xphone-os
../.venv/bin/pio run -e lume-x3-it -e lume-x3-en
```

Risultato osservato:

| Env | RAM PlatformIO | Flash PlatformIO | `firmware.bin` | SHA-256 |
|---|---:|---:|---:|---|
| `lume-x3-it` | 144.996 B | 2.529.849 B | 2.542.384 B | `1f13ec884852424b89694fba276bf8256ef23068a0725fa6d2c525d3f9fef153` |
| `lume-x3-en` | 144.996 B | 2.529.125 B | 2.541.664 B | `fafa18275a9b3e0a707f36ca6d4e4bc2135ac3f4bd6cd56013a876d181637c28` |

Entrambe: `SUCCESS`, RAM 44,2%, flash 38,6%. `esptool image-info` riconosce
ESP32-C3, flash DIO 16 MB e checksum/validation hash validi.

Isolamento verificato cercando byte nei binari: IT contiene
`Impostazioni`, `Nessuna notifica`, `Priorità di oggi`, `GIÙ`, `SÌ` e non le
forme inglesi campione; EN contiene le forme inglesi e non quelle italiane.

Confronto col build upstream dual X3/X4 eseguito prima del fork:

| | Upstream dual X3/X4 | Lume IT | Lume EN |
|---|---:|---:|---:|
| RAM PlatformIO | 145.004 B | 144.996 B | 144.996 B |
| Flash PlatformIO | 2.532.155 B | 2.529.849 B | 2.529.125 B |

I warning rimasti sono ereditati (SdFat/FS macro, JPEGDEC/PNGdec macro,
`NetworkClient::flush` deprecato), non introdotti dalla localizzazione.

## Prova hardware localizzazione italiana — completata

Il firmware `lume-x3-it` è stato caricato via USB su
`/dev/cu.usbmodem11101`. Esptool ha identificato ESP32-C3 revision v0.4,
flash da 16 MB e ha verificato l'hash di ogni regione scritta. Dopo il riavvio
il monitor seriale ha osservato il collegamento BLE cifrato e bonded:

```text
[I][ANCS] Requested low-duty conn params: 90-180 ms, latency=4, timeout=2000 ms
[I][X4CMP] BLE authentication complete conn=1 encrypted=1 bonded=1
[I][X4CMP] BLE link encrypted bonded=1
```

Sul vetro Maurizio ha confermato:

- launcher Lume italiano corretto;
- titoli delle sei app leggibili;
- Impostazioni e una scena applicativa navigabili;
- soft-key, accenti e layout senza difetti visibili.

## Prova hardware v0.1 — completata

Il 13/08/2026 PlatformIO ha rilevato l'X3 su `/dev/cu.usbmodem11101`:

```text
USB VID:PID=303A:1001
ESP32-C3 revision v0.4
MAC d4:05:92:91:72:3c
flash auto-rilevata: 16 MB
```

Ultimo upload hardware pre-localizzazione, eseguito con l'env allora chiamato
`lume-x3`, ha scritto un'immagine da 2.539.728 B: **hash verificato da
esptool**, hard reset completato e `SUCCESS`.
Il monitor, aperto dopo il boot, ha osservato loop stabile, refresh FAST/PARTIAL,
BLE connesso e una notifica WhatsApp completa via ANCS. Maurizio ha confermato
visivamente che il lockup superiore mostra `lume` al posto di `Flowe`.

Checklist di accettazione hardware — completata da Maurizio:

- [x] immagine Lume caricata e hash del flash verificato;
- [x] boot, splash e launcher funzionano sul pannello X3;
- [x] BLE/ANCS operativo: notifica WhatsApp ricevuta, attributi risolti e render eseguito;
- [x] Settings mostra identità/versione Lume;
- [x] About e diagnostica pannello funzionano;
- [x] sleep e dormant frame funzionano;
- [x] scan Bluetooth vede il nome advertising `Lume X3`;
- [x] reader apre e usa gli EPUB;
- [x] riavvio/wake e persistenza si comportano correttamente nei test manuali.

**Esito:** Maurizio ha confermato il 13/08/2026 che, dopo un periodo di prova
manuale, «funziona tutto». v0.1 è accettata; nessuna release pubblica è stata
creata.

## Prova hardware app iOS v0.2 — completata

Il 13/08/2026 Maurizio ha verificato il vertical slice sul telefono e sul vero X3:

- [x] nuova identità Lume trovata via scan e pairing Just Works completato;
- [x] app nello stato `Collegato e pronto`;
- [x] `time.sync` ricevuto dal firmware dopo la connessione;
- [x] priorità `Prova Lume` creata sull'iPhone, richiesta dall'X3 e renderizzata;
- [x] toggle eseguito sull'X3, propagato all'iPhone e rispedito come snapshot;
- [x] reinstallazione della build finale e reconnect automatico riusciti;
- [x] deep-sleep di circa due ore, wake e ripresa normale verificati da Maurizio;
- [x] vecchia app Flowe isolata: dopo UUID e identità BLE dedicati, il seriale
  mostra un solo snapshot `priorities-sync-…`, senza la seconda card `p-…`.

Il test ha anche riprodotto la collisione originale: prima dell'isolamento, Lume
inviava lo snapshot corretto e Flowe rispondeva subito dopo alla stessa notify,
sovrascrivendolo. Cambiare i soli UUID non bastava per via della cache
CoreBluetooth; la nuova identità random-static risolve anche quel percorso.

## Prova hardware Today/EventKit — completata

Il 14/08/2026 è stato completato e verificato il secondo flusso reale dell'app:

- [x] richieste iOS 17+ di accesso completo a Calendario e Promemoria;
- [x] proiezione deterministica delle prossime 24 ore: eventi in corso/futuri,
  promemoria aperti, massimo sei elementi come il `TodayStore` firmware;
- [x] payload `today.snapshot` entro 512 B, con item compatti a cinque campi e
  risposta a `today.sync.request`;
- [x] interfaccia SwiftUI verificata in Simulator sia prima sia dopo i permessi;
- [x] build firmata installata sull'iPhone reale;
- [x] agenda sincronizzata e visualizzata correttamente sul vero X3;
- [x] nuova icona Lume visualizzata correttamente sulla Home dell'iPhone.

Verifiche automatiche: `swift test` con **7/7 PASS** (4 protocollo esistenti +
3 Today); build Simulator e build firmata iPhone entrambe **BUILD SUCCEEDED**.

## v0.4 — orologio hardware DS3231 (completata e accettata sull'X3)

File toccati:

| File | Cosa |
|---|---|
| `xphone-os/src/Ds3231.{h,cpp}` **nuovi** | driver DS3231 a `0x68`: `begin/read/set/oscillatorStopped/readTemperatureTenthsC`, validità da OSF (`0x0F` bit7), control `0x0E = 0x04`, EN32kHz e A1F/A2F spenti, 24 ore forzato, lettura 12 ore convertita |
| `xphone-os/src/ClockStore.{h,cpp}` | ancora `(day, minutesIntoDay)` + `anchorMs`, `fromRtc`, `firstPhoneSyncMs`; `clockNow`, `clockFormatTime`, `clockFormatShortDate` e la matematica civile condivisa |
| `xphone-os/src/reader/ReadingStats.{h,cpp}` | rimossa la copia locale degli algoritmi di Hinnant; `todayYmd()` ora è `clockNow()` |
| `xphone-os/src/main.cpp` | stadio 2.1: seed dal DS3231 dopo il guard X3 e prima del primo disegno, riancorato all'inizio del minuto |
| `xphone-os/src/ble/CompanionBleService.cpp` | `time.sync` riancóra sempre e scrive il DS3231 se lo scarto supera il minuto o OSF era alto |
| `xphone-os/src/scenes/LauncherScene.cpp` | `hh:mm` in status bar a sinistra del pallino BLE |
| `xphone-os/src/Sleep.{h,cpp}` | timbro `dorme dalle hh:mm` sul frame dormiente + commenti "no RTC" corretti |
| `xphone-os/src/scenes/AboutScene.cpp` | riga `orologio: … (rtc\|iPhone)  rtc: ok/OSF/assente` con temperatura |
| `xphone-os/src/BlockStatusStore.h`, `scenes/BlockScene.cpp`, `scenes/PrioritiesScene.cpp` | soli commenti: il countdown Block non si ricostruisce perché la fine arriva come stringa localizzata, non perché manchi l'orologio |
| `xphone-os/test/host/` **nuovo** | suite host (`run.sh`, `clock_test.cpp`, `ds3231_test.cpp`, `ds3231_harness.h`); il workflow di release ora le esegue prima di compilare |
| `xphone-os/update_it.bin`, `update_en.bin` | rigenerati dalle build qui sotto (file locali: `*.bin` è ignorato da Git) |

Scelta divergente dal piano di [12-rtc-e-solo-x3.md](12-rtc-e-solo-x3.md) §1.9:
il driver vive in `xphone-os/src/` e usa i pin di `ACTIVE.batteryGauge` (unico
bus I²C dell'X3), come già fa `BatteryGauge.cpp` per il BQ27220. Non sono stati
toccati né la lib `Rtc` vendorizzata né il literal **posizionale** del profilo
`XTEINK_X3` in `BoardConfig.h`, dove un campo fuori posto compila e assegna il
pin sbagliato (rischio 8 del documento).

Build osservate (`../.venv/bin/pio run -e lume-x3-it -e lume-x3-en`):

| Env | RAM | Flash | `firmware.bin` | SHA-256 |
|---|---:|---:|---:|---|
| `lume-x3-it` | 144.996 B | 2.532.233 B | 2.544.768 B | `c482724faf8ad316eb1f0fd335443895f718e4763efacc06856ee5eeb64cfd2c` |
| `lume-x3-en` | 144.996 B | 2.531.517 B | 2.544.048 B | `db45a80662c7268bf517fd8fd792f19910177c619227d1b593727c5c29f9efc0` |

Delta sulla v0.3: **RAM invariata** (144.996 B), flash +2.384 B (IT) e +2.392 B
(EN). Nessun warning nuovo: restano solo quelli ereditati (SdFat `#warning`,
LTRANS seriale).

Verifiche automatiche eseguite con `sh test/host/run.sh` dalla root del modulo
(le suite vivono in `xphone-os/test/host/`, compilate con `-Wall -Wextra
-Werror`, nessun warning, 4/4 verdi: clock e rtc in IT e in EN):

* `ClockStore.cpp` con `millis()` stubbato: round-trip seriale civile su 41.000
  giorni, mezzanotte, 29 febbraio 2028, capodanno, tre giorni di uptime senza
  sync, e il caso "orologio non impostato". Verde in **entrambe** le locale
  (`ven 14 ago` / `Fri 14 Aug`).
* `Ds3231.cpp` contro un register file DS3231 finto (indirizzamento con
  auto-increment, repeated start, NACK): chip assente → `Absent`; OSF alto →
  `Invalid` con OSF **preservato** da `begin()`; `begin()` su chip pulito non
  scrive nulla; `set()` scrive BCD, 24 ore, secolo 0, giorno-settimana 5 per un
  venerdì e azzera OSF; round-trip identico; registro in 12 ore convertito
  (19:00, 12 AM → 0, 12 PM → 12); mese 13 rifiutato; temperatura 25,25 °C e
  −0,25 °C corrette.

Isolamento locale riverificato sui binari: l'immagine IT contiene
`orologio: …` e `dorme dalle %s`, la EN contiene `clock: …`, `asleep since %s` e
nessuna stringa italiana. L'unico `Aug` nell'immagine IT è il `__DATE__` della
libreria Bluetooth vendorizzata (`Aug 25 2025`), non una fuga di traduzione.

### Prova hardware v0.4 — flash e boot verificati il 14/08/2026

`../.venv/bin/pio run -e lume-x3-it -t upload --upload-port /dev/cu.usbmodem11101`:
scritti 2.544.768 B (1.635.329 compressi), **hash verificato da esptool**, hard
reset eseguito, `SUCCESS`. La porta USB non compariva finché il device dormiva:
è il comportamento noto (il deep sleep spegne il CDC), è bastato il risveglio.

Due boot consecutivi catturati via pyserial con pulse DTR/RTS:

```text
[lume] boot: Xteink X3 confirmed
[lume] rtc: DS3231 2026-08-14 19:22:12
...
[I][X4CMP] time.sync day=20260814 min=1162 connect=2918ms sync=3736ms

[lume] boot: Xteink X3 confirmed
[lume] rtc: DS3231 2026-08-14 19:23:12
```

Cosa prova, letteralmente:

* il nuovo stadio gira **dopo il guard X3 e prima dell'inizializzazione del
  pannello**, come progettato (è la seconda riga del log, prima di `M1 boot`);
* `begin()` ha restituito `Ok`, quindi **OSF era già a 0**: l'oscillatore non si
  è mai fermato su questo esemplare — primo segnale concreto che il tampone c'è;
* il secondo boot legge esattamente un minuto dopo il primo: il chip **conta**,
  non restituisce un valore fisso plausibile;
* `min=1162` del `time.sync` è 19:22, cioè l'ora del chip **coincide col telefono
  al minuto**: il DS3231 di questo X3 teneva già l'ora locale giusta;
* di conseguenza `syncRtcFromPhone()` ha correttamente **non** riscritto nulla
  (nessuna riga `rtc written`): il gate sullo scarto funziona. Il ramo di
  scrittura effettiva resta quindi provato solo dai test host.

**Accettazione sul vetro — confermata da Maurizio il 14/08/2026.** Dopo il flash
ha provato launcher, About e la schermata di sleep e ha riferito che è tutto a
posto: `hh:mm` in status bar, la riga `orologio: … rtc: …` in About e il timbro
`dorme dalle hh:mm` sul frame dormiente. Ha eseguito anche il **test del tampone
VBAT** (power-off e riapertura di About) senza rilevare problemi, quindi il
DS3231 di questo esemplare mantiene l'ora anche senza alimentazione principale.
Nota di precisione: queste ultime conferme sono riferite dall'utente, non
catturate su seriale come le due righe di boot sopra.

Unico ramo mai eseguito su hardware: la **riscrittura** del chip da `time.sync`,
perché lo scarto è sempre stato zero. È coperto dai test host (`set()` con BCD,
24 ore, secolo 0, giorno-settimana e azzeramento di OSF) e si attiverà da solo
la prima volta che il chip devierà di oltre un minuto o perderà l'oscillatore.

## Lotto di correzioni dal backlog (14/08/2026)

Sei voci di [09-cose-da-sistemare.md](09-cose-da-sistemare.md), scelte perché
piccole, indipendenti e utili subito. Ogni voce è marcata RISOLTA nel backlog.

| Voce | Cosa era rotto | Fix |
|---|---|---|
| 15 (P1) | **About sfondava la barra soft-key**: 24 righe da y=16, ultima riga a y=799 su un pannello alto 792, quindi il suggerimento finale era invisibile. La riga `orologio:` di v0.4 ha peggiorato il problema — regressione introdotta in questa sessione | `AboutScene` paginata in due (identità/boot/orologio/heap/refresh, poi power/radio/BLE/stack/hint), soft-key `PREC`/`SUCC`, indicatore `1/2`, `onEnter()` che riparte da pagina 1. Pagina 1 finisce a y=453, pagina 2 a y=420, contro il limite di 748 |
| 13 (P1) | **Campi card troncati a byte**: una priorità con accento o emoji sul limite lasciava una sequenza UTF-8 mozza → mojibake sul vetro. Il percorso ANCS faceva già la cosa giusta con `clipUtf8` | `src/ble/Utf8Clip.h`: unica implementazione, tre overload, usata da entrambi i percorsi. 49 siti convertiti in `CompanionBleService.cpp`, più `BlockStatusStore` (slot 24/16 B contro cap card 96/48 B: il taglio vero avveniva lì) e la sync line di `PrioritiesStore` |
| 20 (P1) | **Statistiche di lettura gonfiate**: `ReadingStats::pageTurn()` era chiamata prima dei controlli di limite, quindi premere avanti a fine libro contava pagine mai girate | conteggio spostato nei soli rami che cambiano pagina; il roll di capitolo conta una volta sola, differito a `workBuildSection()` quando serve indicizzare, e annullato se la build fallisce |
| 16 (P1) | **`_durationCustomized` mai resettato**: dopo un solo tap su `+`, il device ignorava per il resto del boot la durata configurata sul telefono | azzerato in `BlockScene::onEnter()`: la personalizzazione vale per la visita alla scena |
| 19 (P1) | **Hint di Workout con i tasti sbagliati**: diceva left/right (che spostano solo la selezione) mentre i set si contano coi tasti in alto | testo riscritto in IT/EN sui tasti reali |
| 14 (P1) | **Etichette dell'anteprima "Icon style" sfasate**: `kLabels` in ordine diverso dalle colonne di `XPhoneIconPacks`, che è ciò che `iconForApp(i)` indicizza | ordine allineato a `Today, Notifiche, Priorità, Focus, Leggi, Allenamento`, con la fonte citata nel commento. Corretta anche la riga di intestazione di `LauncherIcons.h`, che dichiarava l'ordine sbagliato: era la causa radice |
| 6 (P0) | **`doFlash()` pilotava il pannello senza `waitFlushIdle()`**: un flush e-ink in volo e una scrittura OTA sullo stesso bus SPI, cioè immagine mezza scritta e device che non boota | `SCENES.waitFlushIdle()` + `in.suspendTask()` prima di toccare pannello e SD, `Input::resumeTask()` sul solo percorso che ritorna (flash fallito) |
| 4, 5, 41 (CI) | cache di release che includeva `.pio/build` con chiave sul solo `platformio.ini` (una release poteva contenere oggetti di un altro commit); `XPHONE_VERSION` slegata dal tag; nessuna CI su push/PR | cache ridotta a `~/.platformio`; `XPHONE_VERSION` è ora una macro con default `#ifndef`, iniettata dal tag via `PLATFORMIO_BUILD_FLAGS`; nuovo `.github/workflows/ci.yml` che esegue test host + entrambe le build su push e PR |

Verifica: `sh test/host/run.sh` 4/4 verde, `pio run -e lume-x3-it -e lume-x3-en`
SUCCESS. **RAM invariata** a 144.996 B; flash 2.532.765 B (IT) e 2.532.049 B (EN),
cioè +532 B su ciascuna rispetto alla sola v0.4. Nessun warning nuovo.

L'immagine è stata flashata sull'X3 (hash verificato) e il boot è stato catturato
su seriale: `[lume] rtc: DS3231 2026-08-14 19:50:22`, `time.sync` a `min=1190`
(19:50, ancora concorde al minuto), card `today-sync` e `prio-persist` ricevute e
applicate dal **nuovo percorso di clipping UTF-8** senza errori, loop stabile
(`FULL 3272 ms`, poi `FAST 449 ms`), heap invariata a 122.392 B liberi al boot.

Da guardare sul vetro: le due pagine di About con `PREC`/`SUCC` e la riga finale
interamente visibile; le etichette dell'anteprima Icon style allineate alle icone;
l'hint di Workout; e — con un `.bin` volutamente non valido in Settings →
aggiornamento firmware — i tasti che rispondono di nuovo dopo la X di errore, che
è la prova del `resumeTask()`.

## Prossime azioni

1. Verificare il recovery del timeout GATT scollegando intenzionalmente il
   device durante una write.
2. Creare il repository GitHub personale e aggiungerlo come `origin`, mantenendo
   `upstream` fetch-only.
3. Prossima versione di roadmap: v0.5 dashboard da scrivania, che ora ha l'ora
   vera come prerequisito soddisfatto ([10-scaletta-fork.md](10-scaletta-fork.md)).
4. Non anticipare Screen Time: richiede Apple Developer Program a pagamento.

## Decisioni da non riaprire senza nuova evidenza

Nome Lume; X3-only; tutte e sei le app; italiano/inglese in immagini separate a
compile time; °C/24h; reader Wi-Fi dall'app; payload JSON compatibile ma
identità/UUID GATT Lume isolati; HTTP senza token finché il transfer è effimero;
rollback OTA rinviato; RTC DS3231 implementato e accettato in v0.4; dashboard da
scrivania obiettivo principale.
Dettagli e fonti: [14-decisioni.md](14-decisioni.md).
