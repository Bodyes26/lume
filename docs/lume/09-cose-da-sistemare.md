# Cose da sistemare — lista unica, ordinata per priorità

Aggregazione delle sezioni "Cose da sistemare / attriti" dei capitoli 02–07, più le
issue aperte a monte. Nel codice **non esiste un solo `TODO`/`FIXME`** (verificato:
zero riscontri in `xphone-os/src`): la qualità è alta e i problemi sotto sono
prevalentemente scelte consapevoli, incoerenze doc↔codice e trappole per un fork.

Legenda colonna *Dove*: `FW` firmware, `APP` app iOS (quindi tuo lavoro nuovo),
`CI` build/release, `DOC` documentazione.

## P0 — sicurezza e rischio di perdere il device o i dati

| # | Problema | Dove | Evidenza | Fix |
|---|---|---|---|---|
| 1 | **Card accettate su link non cifrato**: nessun permesso `*_ENC` sulle caratteristiche e `applyCardPayload` non controlla `isEncrypted()`. Un central qualunque può scrivere `transfer.wifi` (SSID+password finiscono in NVS), `transfer.start` (spegne il BLE fino al reboot), `notif.filter` | FW | `src/ble/CompanionBleService.cpp:226-241`, `:834-843` | `WRITE_ENC\|WRITE_AUTHEN` + gate su `isEncrypted()` |
| 2 | **Server HTTP dei libri senza autenticazione**: `/upload`, `/delete`, `/stop`, `/download` di qualsiasi file non nascosto della SD, aperti a tutta la rete | FW | `src/net/FileTransferServer.cpp:32-39,49-89` | token effimero mostrato a schermo/scambiato via BLE, richiesto su ogni endpoint |
| 3 | **Nessun rollback OTA**: si scrive solo `OTA_IMG_NEW`, mai `PENDING_VERIFY` + `mark_app_valid`. Un'immagine valida ma non funzionante è recuperabile solo via USB (cavo pogo 4 pin) | FW | `src/SdUpdate.cpp:365` | scrivere `PENDING_VERIFY` e promuovere al primo boot completo |
| 4 | **Cache CI mai invalidata**: chiave `hashFiles(platformio.ini)` ma path in cache include `.pio/build` → una release può contenere oggetti di un commit arbitrario | CI | `.github/workflows/firmware-release.yml:33-36` | cachare solo `~/.platformio` |
| 5 | **Tag e versione firmware slegati**: `XPHONE_VERSION` è la costante `"0.5.0"`, il tag no. About e `/health` possono mentire | CI | `src/scenes/AppScenes.h:10` | iniettare `-DXPHONE_VERSION` dal tag, o step di verifica che fallisce |
| 6 | **`SettingsScene::doFlash()` pilota il pannello senza `waitFlushIdle()`**: flash firmware avviabile mentre un flush e-ink è in volo (due task sullo stesso SPI) | FW | `src/scenes/SettingsScene.cpp:182` vs contratto `Scene.h:122-124` | aggiungere `SCENES.waitFlushIdle(); input.suspendTask();` |

## P1 — bug funzionali visibili all'uso

| # | Problema | Dove | Evidenza | Note |
|---|---|---|---|---|
| 7 | **ANCS `requestResync()` è un no-op**: l'header promette un latch armato drenato da `pumpResync()`, funzione che non esiste. Aprire Notifications durante la riconnessione post-wake non risincronizza | FW | `src/ble/CompanionAncsClient.h:55-61` vs `.cpp:597-601` | è la causa plausibile di "notifiche che non arrivano dopo il risveglio" |
| 8 | **Stato del blocco dedotto da euristiche su testo inglese** (`"min left"`, `"break"`, `"paused"` in `title`/`body`) | FW+APP | `src/BlockStatusStore.cpp:30-38` | qualunque app localizzata (italiano) rompe la logica → nel fork rendere `state` obbligatorio |
| 9 | **`part`/`parts` onorati solo da Priorities**: Today e Workout sovrascrivono a ogni fetta → resta solo l'ultima | FW | `src/TodayStore.cpp:20-37`, `src/WorkoutStore.cpp:17-49` | oppure rifiutare `parts>1` per quei tipi |
| 10 | **FIFO da 4 slot che scarta il payload più vecchio senza segnalarlo**: una snapshot multi-part può committare una lista sbagliata | FW | `src/ble/CompanionBleService.cpp:690-695`, `src/PrioritiesStore.cpp:26-43` | validare la continuità di `part` |
| 11 | **Nessun ACK/NACK verso il telefono**: card persa, JSON malformato e OOM aggiornano solo una stringa locale | FW+APP | `:680-683`, `:766-771` | notify `card.ack {id,part,ok,reason}` |
| 12 | **Notify che si sovrascrivono**: `setValue`+`notify` senza coda; due comandi nello stesso tick si clobberano (aggirato a mano nel pre-sleep) | FW | `:1152-1178`, `src/Sleep.cpp:337-341` | coda TX drenata su completamento |
| 13 | **Clip dei campi card in byte, non UTF-8-safe**: una priorità con accento/emoji sul confine dei 96 B diventa mojibake (il path ANCS usa correttamente `clipUtf8`) | FW | `:84-91` vs `src/ble/CompanionAncsClient.cpp:178-199` | riusare `clipUtf8` |
| 14 | **Anteprima "Icon style" con etichette sbagliate**: ordine `kLabels` ≠ ordine dei pack | FW | `src/scenes/SettingsScene.cpp:331` vs `src/art/LauncherIcons.h:2274-2276` | bug visibile in Settings |
| 15 | **About sfonda la barra soft-key**: l'ultima riga cade a y≈766, la barra inizia a 748 | FW | `src/scenes/AboutScene.cpp:24-238` | paginare con UP/DOWN |
| 16 | **`_durationCustomized` mai resettato in `onEnter()`**: dopo un tap su `+`, il device ignora per sempre la durata configurata sul telefono | FW | `src/scenes/BlockScene.cpp:189` vs `:161-172` | azzerare in `onEnter()` |
| 17 | **`priority.toggle` senza timeout né stato ottimistico**: se il telefono non risponde, "Updating priority…" resta a schermo per sempre | FW | `src/scenes/PrioritiesScene.cpp:150-163` | transiente 4 s come in Block |
| 18 | **`workout.set` coalescato per indice invece che per id**: se arriva una snapshot che riordina la lista nei 400 ms di attesa, il set finisce sull'esercizio sbagliato | FW | `src/scenes/WorkoutScene.cpp:24,116,31` | memorizzare l'`id` |
| 19 | **Hint di Workout descrive i tasti sbagliati** ("Press left (−) and right (+)" mentre i set si contano con Up/Down laterali) | FW | `src/scenes/WorkoutScene.cpp:224` vs `:153-170` | riformulare |
| 20 | **Statistiche di lettura gonfiate**: `pageTurn()` incrementa prima di verificare i limiti, quindi conta anche i giri pagina non avvenuti | FW | `src/scenes/ReaderScene.cpp:615-637` | spostare la chiamata nei rami che cambiano pagina |
| 21 | **Indicizzazione EPUB bloccante nel main loop**: secondi di freeze su capitoli grandi (input e BLE fermi) | FW | `src/scenes/ReaderScene.cpp:377-385,402-411` | chunkare il parse (expat è già incrementale) |
| 22 | **Rottura tipografica ogni 192 parole**: ultima riga del pezzo non giustificata + rientro spurio a metà paragrafo | FW | `src/reader/ChapterHtmlSlimParser.cpp:150-157`, `src/reader/ParsedText.cpp:426-433` | flush con `includeLastLine=false` |
| 23 | **`progress.bin` riscritto a ogni pagina** (remove+rename su FAT): usura SD e latenza per pagina; il campo `pageCount` salvato non viene mai riletto | FW | `src/scenes/ReaderScene.cpp:923,529-539` | debounce + scrittura all'uscita/sleep |
| 24 | **Nessuna garbage collection della cache reader**: `Epub::clearCache()` non è chiamato da nessuno e la chiave è l'hash del path → cartelle da megabyte orfane dopo rinomina/cancellazione | FW | `src/reader/Epub.cpp:261-274` | sweep all'ingresso della griglia |
| 25 | **Scrittura NVS pesante a ogni sonno** (~2,2 KB di notifiche + 3 JSON + 9 chiavi Block) su una partizione da 20 KB, senza controllare i valori di ritorno | FW | `src/Sleep.cpp:213-308`, `partitions.csv:2` | verificare i ritorni, spostare gli snapshot su SD |
| 26 | **Timeout BUSY del pannello silenziosi**: `waitBusy` esce con `break` dopo 30 s e `PanelDriver::display` non ritorna niente → UI che "sembra" aggiornata | FW | `freeink-sdk/.../EpdBus.cpp:122-161` | far propagare un `bool` |
| 27 | **`TextBlock::deserialize` si fida di `wordCount` fino a 10.000**: su cache corrotta ~120 KB di `resize` → abort/OOM invece di "cache invalida" | FW | `src/reader/TextBlock.cpp:57-68` | limite realistico (256) |
| 28 | **Prewarm font con `malloc` fino a ~30 KB contigui in fase di render**; se fallisce degrada in silenzio a una inflate per glifo (pagine lente) | FW | `lib/EpdFontCore/FontDecompressor.cpp:465-472` | buffer persistente dimensionato al gruppo massimo |
| 29 | **Immagini interne degli EPUB scartate senza segnaposto** | FW | `src/reader/ChapterHtmlSlimParser.cpp:244-249` | emettere un box con la `alt` |
| 30 | **Ramo X4 dell'artwork Block irraggiungibile** (`#if FREEINK_DEVICE_X3` sempre vero nel binario universale): su X4 si disegna l'hero dimensionato per X3 | FW | `src/art/BlockArtwork.h:20` vs `platformio.ini:99-115` | scelta a runtime su `gDeviceIsX3` |

## P2 — attriti per il fork, codice morto, doc fuori sincrono

| # | Problema | Dove | Evidenza |
|---|---|---|---|
| 31 | **Gli script che generano icone e artwork non sono nella repo pubblica** (`tools/xphone-icons/`, `tools/x4-block-assets/`, `tools/x4-screen-lab` citati ma assenti): non puoi rigenerare la UI senza riscriverli. Formato noto: 104×104, 1 bpp MSB-first | FW | `src/art/LauncherIcons.h:4`, `src/art/BlockArtwork.h:7-8` |
| 32 | **`subset_epd_font.py` punta a input del monorepo privato** (`../x4-os/lib/EpdFont/builtinFonts/*.h`) e il generatore dei font reader (`fontconvert.py`) non c'è → aggiungere glifi (cirillico, greco) è bloccato | FW | `tools/subset_epd_font.py:26-32` |
| 33 | **DS3231 presente sull'X3 e mai usato**: viene interrogato solo come fingerprint di identità, mentre il firmware ripete "no RTC" per giustificare l'assenza di orologio. È l'occasione più grossa del fork (clock e countdown veri senza telefono) | FW | `freeink-sdk/.../XteinkDetect.cpp:60-66`, `BoardConfig.h:464`, `src/Sleep.cpp:88-91` |
| 34 | **`RecoveryBoot` è codice morto** e il combo reale sarebbe Back+Up, non Left+Power come dice la doc | FW | `freeink-sdk/.../RecoveryBoot.cpp:36`, assente da `platformio.ini:83-90` |
| 35 | **Partizioni `spiffs` (3,375 MB) e `coredump` dichiarate e mai usate**, mentre gli slot app sono così stretti da richiedere `-flto` | FW | `partitions.csv` |
| 36 | **Percorso "mail" morto**: `mailItems`/`mailSource`/`mailSync` parsati (8 item × 6 stringhe) e letti da nessuno | FW | `src/ble/CompanionBleService.cpp:910-931` |
| 37 | **`actions[]` + `sendAction()` + `card.action` senza chiamanti** | FW | `:1031-1066` |
| 38 | **`MAX_CARD_BYTES = 4096` irraggiungibile**: un write singolo è ≤ `ATT_MTU−3` (~514 B) e non c'è long write | FW | `src/ble/CompanionProtocol.h:36` |
| 39 | **Launcher che smista le app con `strcmp` sull'etichetta**: rinominare un tile rompe la navigazione | FW | `src/scenes/LauncherScene.cpp:127-140` |
| 40 | **`x3`/`x4`/`xteink` compilano lo stesso binario ma la release pubblica due file** e lo zip esiste solo per X3 | CI | `platformio.ini:95-124`, `firmware-release.yml:57,71-83` |
| 41 | **Nessuna CI su push/PR**: si scopre che il codice non compila solo al momento del tag | CI | `firmware-release.yml:3-12` |
| 42 | **La patch al framework BLE scrive nella directory PlatformIO globale**: contamina tutti gli altri progetti della macchina | CI | `tools/patch_ble_service_friend.py:24-27` |
| 43 | **expat vendorizzato alterato senza file di patch tracciabile** (`#error` sull'entropia commentato) | FW | `lib/expat/xmlparse.c:145-160` |
| 44 | **README stale su più punti**: nome BLE annunciato (`X4 Companion` vs `xphone X3`), costo font (81.853 vs ~41.391 B), `NotificationStore` (32×232 vs 24×224), tile 158/142 px vs `kMaxTileSide=186`, scene "parked for M2" già implementate, `pio run -e x4` elencato due volte con stati hardware contraddittori | DOC | `README.md:87,107-109,120,239-254,289` |
| 45 | **Nessun test eseguibile**: l'unico smoke test è gated da `-DXP_READER_SMOKE`, macro che nessun env definisce | FW | `src/reader/ReaderSmokeTest.h:3-6` |
| 46 | Altro codice morto e commenti errati: `FlushReq::Flash`/`Gfx::flushWindowFlash` irraggiungibili, `Gfx::drawRect` senza call-site, `if` vuoto in `IconStyle.cpp:43-45`, commenti font "solo ASCII" mentre i subset coprono Latin-1 + Latin Ext-A, `LauncherScene.h:3` "3x2" con `COLS=2` | FW | vari |

## Issue aperte a monte (github.com/andrewjiang/flowe-os)

Utili perché dicono cosa l'autore stesso considera rotto o mancante:

| Issue | Titolo | Rilevanza per te |
|---|---|---|
| [#32](https://github.com/andrewjiang/flowe-os/issues/32) | Block breaks: shield never re-applies unless the app is reopened | bug lato app iOS — da non replicare nella tua |
| [#24](https://github.com/andrewjiang/flowe-os/issues/24) | Priorities: wrap long lines instead of truncating | firmware, fix piccolo e di alto impatto |
| [#4](https://github.com/andrewjiang/flowe-os/issues/4) | Show temperature in Celsius | **si risolve nella tua app**: il device riceve stringhe già formattate |
| [#3](https://github.com/andrewjiang/flowe-os/issues/3) | Cyrillic → `?` | i font UI coprono solo Latin-1 + Latin Ext-A: gli accenti italiani vanno bene |
| [#31](https://github.com/andrewjiang/flowe-os/issues/31) | Hebrew books | manca RTL/bidi nel reader; non ti serve |
| [#1](https://github.com/andrewjiang/flowe-os/issues/1) | Can't connect to wifi | il Wi-Fi si configura **solo** via card BLE `transfer.wifi`: la tua app deve implementarla |
| [#11](https://github.com/andrewjiang/flowe-os/issues/11) | Open source the iOS app | non è successo: la tua app va scritta da zero |
| #13, #14, #19, #20, #25, #26, #27, #28, #30 | read-to-unblock, flashcards, contatore set aperto, tempo in Block, fixation-bold, cartella Done, habits, dashboard | idee già validate dall'autore, buon serbatoio per le "tue" feature |
