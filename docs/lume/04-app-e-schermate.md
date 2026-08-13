# App e schermate di Flowe OS

Dieci scene statiche in `xphone-os/src/scenes/`, istanziate una volta sola come `static` in `AppScenes.cpp:24-34` (zero heap, nessuno stack di scene). Navigazione tramite gli helper `show*()` di `AppScenes.h:47-63`, che aggiornano `gCurrentSceneId` (usato da `Sleep.cpp` per il restore e da `main.cpp` per il resync). Per il protocollo BLE vedi [03-protocollo-ble.md](03-protocollo-ble.md); per il motore EPUB [05-reader-epub.md](05-reader-epub.md); per sleep/OTA/NVS [06-hardware-power-ota.md](06-hardware-power-ota.md).

## Chrome comune: soft-key, tasti, font

Sei tasti logici (`Input.h:45`): `Up`/`Down` = coppia sul **bordo superiore** (side, ladder ADC GPIO2), `Back`/`Confirm`/`Left`/`Right` = quattro tasti **frontali** in basso (front, ladder ADC GPIO1). Tap riportato al **rilascio**, long-press a `kLongPressMs = 550` (`Input.h:56`) e consuma il tap. Il tasto POWER non arriva alle scene.

La barra soft-key la disegna `SceneManager` dopo ogni `render()` (`Scene.cpp:100-107`): 4 tab arrotondate in alto, altezza visibile `kTabH = 28` su una fascia riservata `Scene::SOFTKEY_BAR_H = 44` (`Scene.h:19`); margine laterale 8% (`kBarMarginPct`, `Scene.cpp:28`), gap 10, quindi su X3 `marginX = 42`, `tabW = (528-84-30)/4 = 103`, `tabY = 792-28 = 764` (`Scene.cpp:56-64`). Ordine slot **fisso**: `0 = Back`, `1 = Confirm`, `2 = Left`, `3 = Right`; `nullptr` = tab nascosta. Un puntino di 3 px sopra la tab marca gli slot con long-press (`longPressSlots()`, default `0x01`). Lo slot con bit in `softKeyIconMask()` diventa una tab a metà larghezza con l'ingranaggio disegnato da primitive (`Scene.cpp:34-51`).

**Convenzione OS**: long-press `Back` = torna al launcher da qualsiasi scena, intercettato in `SceneManager::loop` prima di `handleInput` (`Scene.cpp:109-120`); la scena non lo vede mai.

Font (`Fonts.cpp:19-46`): `kFontRegular`/`kFontBold` = ubuntu_12 ASCII (advanceY **29**, ascender 24), `kFontSmall` = ubuntu_10 (advanceY **24**, ascender 20). Sottoinsiemi ASCII: i codepoint non-ASCII rendono `?`.

Header standard delle app-card: titolo bold a `x = 20`, `y = 8`, riga separatrice `fillRect(0, kHeaderH-2, w, 2)` con `kHeaderH = 46`.

---

## Launcher — `scenes/LauncherScene.cpp`

**Scopo**: griglia delle 6 app + status bar. Scena di default al boot e bersaglio di ogni long-press `Back`.

**Ordine tile esatto** (`LauncherScene.cpp:22-24`): `Today`, `Notifications`, `Priorities`, `Block`, `Read`, `Workout`. `COLS = 2`, `APP_COUNT = 6` (`LauncherScene.h:17-18`) → griglia **2 colonne × 3 righe**.

**Geometria** (`LauncherScene.cpp:48-55`, `200-232`): `kMargin 16`, `kStatusH 40`, `kGap 28`, `kBoxInset 8`, `kTilePad 12`, `kSelRadius 12`, `kSelThick 3`, cap `kMaxTileSide 186`. Lato tile = `min((w-2·16-28)/2, (h-44-40-2·28)/3)` limitato a 186; su X3 e X4 il cap vince sempre → tile 186×186, blocco 400×614, `gridX = (w-400)/2` (X3: 64), `gridY = 40 + (availH-614)/2` (X3: 87). Bordo arrotondato disegnato **solo** sulla tile selezionata; label dentro il box, ancorata in basso (`boxY + boxSide - 12 - 29`), bold se selezionata, regular altrimenti.

**Icone**: bitmap 1bpp **104×104**, MSB-first, bit 0 = inchiostro, generate da `tools/xphone-icons/build_launcher_icons.py` (`art/LauncherIcons.h:4-9`). 5 pack (`Current`, `Stamp`, `Cutout`, `Etched`, `Signal`) × 6 app = 30 bitmap in flash, tabella `XPhoneIconPacks` a `LauncherIcons.h:2274-2280`. Pack attivo da `IconStyle::iconForApp()`, indice persistito in NVS namespace `xphone`, chiave `iconPack` (`IconStyle.cpp:11-12`). Il blitter disegna a scala nativa 1:1 (`LauncherScene.cpp:34-45`); lo stride è calcolato sul master (104), non su `size` — regressione già corretta e documentata in loco.

**Status bar** (`LauncherScene.cpp:162-201`): lockup "flowe" (semisole 22 px + onde) a sinistra; a destra cluster batteria CrossPoint 15×12 + nub, riempimento proporzionale, percentuale in `kFontSmall`, `--%` se ignota (`StatusBar.h:28-70`); fulmine di carica solo se `BatteryGauge::readAvgCurrentMa() > 0` (X3, BQ27220). Punto BLE 12 px a sinistra della batteria: pieno se connesso, cerchio vuoto se in advertising, assente prima dell'avvio radio. Punto di sync 5 px a sinistra della percentuale, pieno/vuoto secondo `xphoneSyncBusy()` (`StatusBar.h:96-110`).

| Tasto | Azione | Soft-key |
|---|---|---|
| `Left` (front, slot 2) | selezione precedente, wrap Workout↔Today | `PREV` |
| `Right` (front, slot 3) | selezione successiva, wrap | `NEXT` |
| `Up` (side) | riga sopra (clamp) | — |
| `Down` (side) | riga sotto (clamp in ultima riga corta) | — |
| `Down` long-press (side) | `showBlockDeepWork()`: apre Block e lancia subito `block.start(deep_work, 30)` | — (nessuna tab, nessun puntino) |
| `Confirm` (slot 1) | apre l'app selezionata | `OPEN` |
| `Back` tap (slot 0) | apre **Settings** | icona ingranaggio (`softKeyIconMask() = 0x01`) |
| `Back` long-press | no-op (già home) | puntino su slot 0 |

**Stato persistito**: nessuno. `_sel` vive nell'istanza statica e riparte da 0 (`Today`) a ogni boot/wake.
**Wake**: il launcher è la scena di fallback di `showSceneById()` (`AppScenes.cpp:117-131`). `main.cpp:378` e `:524` lo ridipingono sui soli edge di connessione BLE e di attività sync (aggiornamento dei due punti).
**BLE emessi**: nessuno diretto; il long-press `Down` delega a BlockScene.

---

## Notifications — `scenes/NotificationsScene.cpp`

**Scopo**: inbox ANCS, due viste interne (`List`, `Detail`), nessuno stack di scene.

**Dati**: `NOTIFICATION_STORE` (`NotificationStore.h:23-51`) — ring **24 voci** ordinate per `sortKey` (data ANCS), `get(0)` = più recente; campi fissi `appId 32`, `title 56`, `message 112`, `Entry` = 224 B → 5 376 B BSS. Il nome app leggibile arriva da `COMPANION_ANCS.appDisplayName()` al render (lo store tiene il bundle id). Al sleep vengono salvate le **10** più recenti come blob NVS `notifStore` + `notifTombs` (`Sleep.cpp:69-71`).

**Layout lista**: `rowHeight = 29+29+14 = 72` (`:39`); righe per pagina `= (h-46-44-8)/72` → **9** su X3 e X4 (`:85-89`). Riga: titolo bold a `x=20` + nome app regular allineato a destra (clip a `textW/3`), messaggio regular sotto, separatore 1 px se non selezionata; cursore = bordo arrotondato 3 px a `x=8` (`:315-318`). Pillola SYNC in header alto a destra, `kSyncPillH = 26`, tre stati: contorno 1 px, contorno 3 px se il cursore è su di lei (`_sel == -1`), invertita con testo `SYNCING` per `kSyncFlashMs = 1200` dopo la pressione (`:277-295`).

**Layout dettaglio** (`:347-388`): "Notification" + `n of m`, riga nome app + filetto, titolo bold wrappato max 3 righe, messaggio wrappato fino al fondo con `...` se tagliato.

| Tasto | Lista | Dettaglio |
|---|---|---|
| `Left`/`Up` | su; da riga 0 salta sulla pillola SYNC (`_sel = -1`) | notifica precedente (clamp) |
| `Right`/`Down` | giù; da SYNC rientra su riga 0 | notifica successiva (clamp) |
| `Confirm` | `OPEN` se su riga; `SYNC` = `COMPANION_ANCS.requestResync()` se `_sel == -1` | `CLEAR`: `removeAt` + `recordTombstone` + `dismissNotification` (`:170-177`) |
| `Confirm` long-press | `clearAll()` di tutta l'inbox, solo con riga selezionata (puntino su slot 1, `:77-83`) | — |
| `Back` | launcher | torna alla lista, selezione preservata |

Soft-key (`:62-75`): lista `BACK/OPEN/UP/DOWN`; cursore su SYNC `BACK/SYNC/UP/DOWN`; inbox vuota `BACK/SYNC/-/-`; dettaglio `BACK/CLEAR/PREV/NEXT`.

**Stato persistito**: le voci e i tombstone (NVS, vedi sopra); `_view`/`_sel` no.
**Wake**: `onEnter()` (`:48-60`) mette `_sel = 0` (o `-1` a inbox vuota) e lancia sempre `requestResync()`. Le voci NVS sono re-iniettate al boot da `Sleep::seedPersistedBlock()` (`Sleep.cpp:471-493`), quindi la lista è già popolata prima che ANCS riconnetta.
**BLE emessi**: `requestResync()`, `dismissNotification()` (azioni ANCS, non card companion).

---

## Priorities — `scenes/PrioritiesScene.cpp`

**Scopo**: to-do del giorno sincronizzate dall'iPhone; mirror del toggle verso il telefono.

**Dati**: `PRIORITIES_STORE`, capacità **10** (`PrioritiesStore.h:41`), item `id 65` / `title 97` / `note 121` / `done` = 284 B; riempito dal servizio BLE per **ogni** card `priorities.snapshot` (o id con prefisso `priorities-sync-`), anche a scena chiusa (`CompanionBleService.cpp:984-986`). Snapshot multi-parte: gli slice si accumulano e `count`/`syncLine`/`revision` si committano solo sull'ultima parte (`PrioritiesStore.cpp:20-42`).

**Layout** (`:208-325`): header + sotto-header "Today" a `y=56` con "N active / M done" a destra; riga transient a `syncY = 87`; `rowH = 29+24+22 = 75`, `listTop = 123`, `perPage = (h-44-8-123)/75` → **8** su X3/X4; indicatore `x-y of N` a destra della riga sync quando `count > perPage`. Riga: checkbox 32×32 (raggio 8, bordo 3 se selezionata, 2 altrimenti, spunta a due tratti con `drawLine`, `:64-70`), titolo (+ nota in `kFontSmall` solo se `noteIsReal()`, che scarta `none`/`n/a`/`-`, `:43-61`), card selezionata con bordo arrotondato 3 px. Vuoto: "Today's priorities" + `Requesting priorities...` / `Syncing...` / `Connect Companion to sync.`.

| Tasto | Azione | Soft-key |
|---|---|---|
| `Left`/`Up` | selezione su | `UP` |
| `Right`/`Down` | selezione giù | `DOWN` |
| `Confirm` | lista piena: `sendPriorityToggle(id, !done)` (`:157`); lista vuota: `sendPrioritiesSyncRequest()` | `DONE` / `SYNC` |
| `Back` | launcher | `BACK` |

**Stato persistito**: la lista è serializzata **dallo store** (non dalla card) in NVS `prioCard` al sleep (`Sleep.cpp:251-268`) e reiniettata al boot come card sintetica `prio-persist` (`Sleep.cpp:464-465`).
**Wake**: `onEnter()` (`:123-137`) azzera selezione e richiede lo snapshot; `main.cpp:470` la ridisegna sui bump di revisione.
**BLE emessi**: `priorities.sync.request`, `priority.toggle`.
**Extra**: questa scena esporta le tre facce dormant condivise (`renderDormant`, `renderDormantBlockLine`, `renderDormantFooter`, `:333-470`) usate dalla schermata di sleep.

---

## Today — `scenes/TodayScene.cpp`

**Scopo**: agenda + reminder + meteo del giorno, sola lettura.

**Dati**: `TODAY_STORE`, capacità **6** item (`TodayStore.h:32`), campi `kind 65` / `time 49` / `title 97` / `subtitle 49`; più `weather`, `highLow`, `syncLine` (97 B ciascuno). Predicato di cattura: `kind == "today.snapshot"` o id `today-sync-*` (`CompanionBleService.cpp:994-997`). `kind == "reminder"` finisce nella sezione Reminders, tutto il resto è agenda.

**Layout** (`:13-46`, `:194-346`): banda meteo opzionale `kWeatherBandH = 10+24+8 = 42` (condizione regular a sinistra, `H/L` small a destra); righe appiattite in `Row[MAX_ROWS = 14]` da `buildRows()` (`:118-148`): divisore giorno (icona sole/luna 22 px + label `TONIGHT`/`TOMORROW` dal `subtitle`), header `REMINDERS` con campanella, item. Altezze: divisore/sezione `24+28 = 52`, item `22+30+18 = 70`. Item: ora in `kFontSmall` sopra, titolo bold sotto; i reminder hanno checkbox vuota 22 px e rientro `kRemGutter = 35`. Header: "Today" + `syncing...` oppure `TODAY_STORE.syncLine()` (es. "Synced 9:41 AM"). Messaggio transient in basso a sinistra.

| Tasto | Azione | Soft-key |
|---|---|---|
| `Left`/`Up` | scroll su di una riga | `UP` |
| `Right`/`Down` | scroll giù fino a `_maxScrollCache` | `DOWN` |
| `Confirm` | `sendTodaySyncRequest()` | `SYNC` |
| `Back` | launcher | `BACK` |

Soft-key: `BACK/SYNC/UP/DOWN` con snapshot, `BACK/SYNC/-/-` senza (`:163-167`).

**Stato persistito**: ultima card JSON in NVS `todayCard`, salvata al sleep (`Sleep.cpp:246-247`) e reiniettata al boot.
**Wake**: `onEnter()` azzera lo scroll e richiede la sync; `main.cpp:479` marca dirty sui bump di revisione. La card seed rende visibile l'agenda cachata prima della riconnessione.
**BLE emessi**: `today.sync.request` — che secondo `CompanionBleService.h:118-122` **non ha handler** nell'app iOS esistente: per il fork iOS è un tipo da implementare.

---

## Block — `scenes/BlockScene.cpp`

**Scopo**: attivare/fermare gli Screen Time shield sull'iPhone. Il telefono è l'autorità; il device mostra e comanda.

**Preset** (`:25-30`): `deep_work`/Deep Work/30, `reading`/Reading/45, `evening`/Evening/60, `workout`/Workout/60. Durata `10..180` a passi di `10` (`:46-48`). Scelte break (`:37-41`): `Keep blocking`, `5 min break`, `Stop now`.

**Macchina a stati.** Tre viste (`View::Main|Modes|Break`) più una macchina di transienti/ottimismo ortogonale:

- `_activeCache` decide Main-ready vs Main-active, calcolato in `render()` (`:649-650`): `optimisticReady ? false : (cardActive || optimisticActive || seedActive)`.
- `Pending::{Start,Stop,Break}` armato all'invio comando; risolto dalla card fresca (`:611-643`) o, dopo `kTransientTimeoutMs = 4000` (`:67`), cadendo nello stato ottimistico (`:267-291`) — così la UI non resta bloccata su "Starting...".
- **Countdown locale** senza costo radio: `latchEnd(minuti)` fissa `_endMs = millis() + min·60000` (`:201-205`), `remainingNowMin()` è rollover-safe e arrotonda per eccesso (`:210-214`); `tickTransients` marca dirty **una volta al minuto** (`:294-295`).
- **Zero locale**: al raggiungimento di 0 invia un solo `block.status`; se nessuna card arriva entro 4 s mostra ready ottimistico e "Block finished." (`:297-311`).
- **Seed da NVS** (wake con blocco attivo): `fromCard == false` → nessun countdown, si mostra l'etichetta assoluta `until <endsAtLabel>` più "updating..." (`:532-546`).

**Stato mostrato** dallo store `BLOCK_STATUS` (`BlockStatusStore.h:31-55`, ~64 B): `active`, `onBreak`, `fromCard`, `ready`, `remainingMinutes`, `durationMinutes`, `preset[24]`, `endsAtLabel[16]`, `blocksToday`, `streak`, `total`. Riempito solo per card con `id == "block-status"` (`CompanionBleService.cpp:1010-1012`); euristiche legacy (`state`, `"min left"`, `"paused"`) in `BlockStatusStore.cpp:29-45`.

**Layout ready** (`:428-483`): tab `-`/`+` sui bordi laterali, `kSideTabW 26`, `kSideTabH 141`, `kSideTabY 119`, disegnate 10 px oltre il bordo così solo gli angoli interni restano arrotondati; pannello centrale alla stessa quota (`panelX = 40`, `panelW = w-80`, raggio 18, bordo 3) con icona preset 48×48, titolo bold, divisore verticale a `panelW-96` e lettura `"30m"`. Sotto: messaggio locale centrato, poi — se `total > 0 || blocksToday > 0` — la riga `Today N      Streak N` in `kFontSmall`, poi l'illustrazione "field" centrata (X3 **480×330** = 19 800 B, X4 432×297 = 16 038 B; 1bpp, `art/BlockArtwork.h:11-22`).

**Layout active** (`:485-547`): titolo preset a `kTitleY = 70`, hero "WORK" centrata fra titolo e pannello, pannello countdown `panelX = 50`, `panelW = w-100`, `panelH = 122`, `panelY = h-44-68-122-46` (X3: 512): numero minuti bold, label `min left`/`min break`, barra progresso `barW = panelW-40` alta 8 con riempimento `(duration-remaining)/duration` (non disegnato durante il break).

**Layout modes/break** (`:549-595`): 4 righe da 76 px (gap 12) con icona preset (dumbbell disegnato a rettangoli per `workout`, `:142-148`) e sottotitolo `"N min"`, pallino pieno 22 px sulla selezionata; break = 3 righe da 62 px (gap 18), selezionata **invertita** (fondo nero, testo bianco).

| Tasto | Main ready | Main active | Modes / Break |
|---|---|---|---|
| `Up` (side) | durata −10 min | ignorato | selezione su |
| `Down` (side) | durata +10 min | ignorato | selezione giù |
| `Left` (slot 2) | apre Modes | apre chooser Break | selezione su |
| `Right` (slot 3) | — | — | selezione giù |
| `Confirm` (slot 1) | `block.start(durata, presetId)` | non associato (lo stop passa da "Stop now") | Modes: adotta preset+durata e torna a Main; Break: esegue la scelta |
| `Back` (slot 0) | launcher | launcher | torna a Main |

Soft-key (`:174-183`): ready `BACK/START/MODE/-`, active `BACK/-/BREAK/-`, liste `BACK/SELECT/UP/DOWN`.

**Interazione Screen Time**: il device non blocca nulla in locale; invia `block.start` / `block.break(5)` / `block.stop` / `block.status` e l'iPhone risponde con una card `block-status` fresca dopo ogni comando. Conteggio giornaliero, streak e totale **arrivano dalla card** (`blocksToday`/`blockStreak`/`blocksTotal`) e sono persistiti a ogni sleep nelle chiavi NVS `blkToday`, `blkStreak`, `blkTotal`, indipendentemente dallo stato attivo (`Sleep.cpp:240-242`, reseed `:457-458`). Lo snapshot del blocco attivo usa `blkActive`, `blkBreak`, `blkRemain`, `blkDur`, `blkPreset`, `blkEnds` (`Sleep.cpp:50-55`).
**Wake**: `onEnter()` (`:161-172`) richiede `block.status`; `Sleep::seedPersistedBlock()` semina lo stato prima che il BLE torni su, così la vista "locked" appare subito. `BLOCK_STATUS.active()` allunga anche il timeout di auto-sleep (`main.cpp:600-602`).

---

## Workout — `scenes/WorkoutScene.cpp`

**Scopo**: contare le serie set-by-set; struttura decisa dal telefono, incremento sul device.

**Dati**: `WORKOUT_STORE`, capacità **8** esercizi (`WorkoutStore.h:29`), item `id 65` / `name 49` / `sets` (0..99) / `done` (0..sets), più `date[17]`. Card `workout.snapshot` o id `workout-sync-*` (`CompanionBleService.cpp:1000-1003`). Guardia anti-regressione: a parità di `date`, `done = max(locale, incoming)` per id corrispondente (`WorkoutStore.cpp:19-47`).

**Layout** (`:173-296`): header + "Today"/"Workout complete!" con `N / M done` a destra; riga suggerimento `Press left (-) and right (+) to count sets.`; `rowH = 75`, `perPage = 8`. Riga: checkbox 32 px (spuntata quando `done >= sets`), nome, contatore `"3/10"` allineato a destra, seconda riga = barra di pip 12×12 con passo 18 se `sets <= kMaxPips (12)`, altrimenti didascalia `"N sets"`/`"done"`.

**Invio coalescato** (`:24-35`, `:111-126`): `bumpDone` aggiorna subito lo store (repaint ottimistico) e arma `s_pendingIdx` con scadenza `kSendQuietMs = 400` ms; alla scadenza (o su `onExit`, o su `flushPendingSend()` chiamato da `Sleep.cpp:202`) parte **un solo** `workout.set(id, doneAssoluto)` — idempotente.

| Tasto | Azione | Soft-key |
|---|---|---|
| `Up` (side) | −1 serie sull'esercizio selezionato | — |
| `Down` (side) | +1 serie | — |
| `Left` (slot 2) | selezione su | `UP` |
| `Right` (slot 3) | selezione giù | `DOWN` |
| `Confirm` | lista piena: +1 serie; lista vuota: `workout.sync.request` | `+SET` / `SYNC` |
| `Back` | launcher (con flush del pending in `onExit`) | `BACK` |

**Stato persistito**: NVS `wkCard`, serializzato **dallo store** al sleep (`Sleep.cpp:273-290`) per non perdere le serie contate offline.
**Wake**: `onEnter()` azzera selezione e chiede la sync; `main.cpp:489` marca dirty su revisione. Se il sonno parte da questa scena la faccia dormant è la lista workout (`renderDormant`, `:298-357`).
**BLE emessi**: `workout.sync.request`, `workout.set`.

---

## Reader — `scenes/ReaderScene.cpp`

**Scopo**: lettura EPUB; qui solo scene/tasti, motore e cache in [05-reader-epub.md](05-reader-epub.md).

Cinque stati (`ReaderScene.h:50`): `Opening`, `Indexing`, `Reading`, `BookList`, `Error`. Il lavoro bloccante (inflate, parse, impaginazione, cover) non gira mai in `onEnter()`/`render()`: `render()` compone il frame di avviso e arma `Work`, che `handleInput()` esegue quando il flush worker è idle (`:562-570`).

**Layout lettura**: `kMarginX/kMarginTop = 24`, striscia di stato `kStatusH = 24` sopra la barra soft-key, con l'unica chrome `"34%  ·  ch 3/12"` (il punto centrale è un quadrato 3×3 perché i font sono ASCII, `:927-950`).
**Layout scaffale**: griglia **2×2** (`kGridCols/kGridRows = 2`), banda statistiche `kStatsBandH = 56` fra header e tile, thumb target 200×260, bordo selezione inset 6 / raggio 10 / spessore 3 (`:89-105`). La banda legge `reader::ReadingStats::band()` (streak giorni, pagine di oggi, barre Lun–Dom; `reader/ReadingStats.h:29-38`), da `/.xphone/stats.bin` (ring 64 giorni + 64 libri, ~1,5 KB).

| Tasto | Reading | BookList | Error |
|---|---|---|---|
| `Right`/`Down` | pagina avanti | +1 libro / +1 riga (`Down` = `+kGridCols`) | — |
| `Left`/`Up` | pagina indietro | −1 libro / −1 riga | — |
| `Confirm` | `SIZE`: cicla 12/14/16 pt mantenendo la posizione per rapporto | apre il libro selezionato | — |
| `Back` | va allo scaffale | launcher | scaffale |

Soft-key (`:262-280`): Reading `BOOKS/SIZE/PREV/NEXT`, scaffale `BACK/OPEN/UP/DOWN` (vuoto: `BACK/-/-/-`), frame `Opening`/`Indexing` **senza tab**.

**Stato persistito**: NVS `rdBook` (percorso ultimo libro) e `rdFont` (dimensione), più `progress.bin` per capitolo/pagina salvato a ogni pagina (`:9-10`, `:923`).
**Wake**: `SceneId::Reader` è ripristinabile; in quel caso `main.cpp:300-302` **rimanda** l'avvio delle radio, che riprendono all'uscita dal reader. `onExit()` chiama `ReadingStats::sessionEnd()` (`:242`).
**BLE emessi**: nessuno; la scena sospende la radio per il lavoro sul libro.

---

## File Transfer — `scenes/FileTransferScene.cpp`

**Scopo**: portare i libri sul device via Wi-Fi + HTTP. BLE e Wi-Fi non convivono (X3 idle ~39 KB liberi, `esp_wifi` ne chiede ~50): l'attivazione **spegne il BLE** (`:106-108`).

**Stati** (`FileTransferScene.h:41`): `Idle → Connecting → Running | Failed`. `kStaTimeoutMs = 20000`, `kFailedLingerMs = 6000`, `kPumpPerTick = 8` `handleClient()` per tick, hostname/mDNS `xphone` (`:17-30`, `:164-166`).

**Dati mostrati**: SSID salvato (da `WifiCreds::load`, NVS — provisioning solo via BLE `transfer.wifi`), IP, `http://<ip>/`, contatore richieste e KB trasferiti aggiornato solo su cambio richieste o ogni 256 KB (`:230-238`).

| Tasto | Idle | Connecting | Running | Failed |
|---|---|---|---|---|
| `Confirm` | `SYNC`: join STA (solo con SSID salvato) | — | — | `RETRY`: rilancia il join |
| `Back` | esce | `CANCEL`: disconnette e torna a Idle | `EXIT`: stop server + restart | esce |

Soft-key (`:59-72`): `BACK/SYNC/-/-`, senza credenziali `BACK/-/-/-`, `CANCEL/-/-/-`, `EXIT/-/-/-`, `BACK/RETRY/-/-`.

**Uscita**: se la radio è mai salita, l'uscita è un `esp_restart()` — unico teardown pulito dopo Wi-Fi (`:45-57`, `:179-189`). Un fallimento a radio spenta si auto-riavvia dopo 6 s per far tornare il BLE (`:251-255`).
**Wake**: `SceneId::FileTransfer` è **esclusa** dal restore (`AppScenes.cpp:123-126`) e azzera il timer di auto-sleep mentre è attiva (`main.cpp:598`).
**BLE emessi**: notify `transfer.status` con stati `connecting`, `running`, `stopped`, `needs-wifi`.

---

## Settings — `scenes/SettingsScene.cpp`

Cinque viste interne (`Menu`, `Picker`, `ConfirmFlash`, `ConfirmRestart`, `IconStyle`). Stile CrossPoint: header titolo + versione (`XPHONE_VERSION = "0.5.0"`, `AppScenes.h:12`), righe alte `29 + kRowPad(16) = 45`, barra selettore **piena a tutta larghezza** con testo invertito (`:273-285`).

| # | Voce (`:38`) | Effetto | Default |
|---|---|---|---|
| 0 | `File Transfer` | apre la scena File Transfer | — |
| 1 | `SD Firmware Update` | monta SD, elenca `*.bin` e `*.bin.flashed` nella root (max `MAX_BIN_FILES = 16`, nomi ≤ 64 char), conferma → `sd_update::flashFromPath` su slot OTA inattivo + restart | — |
| 2 | `Icon style` | vista di anteprima; `PREV`/`NEXT` ciclano i 5 pack applicandoli subito e scrivendo NVS `iconPack` | pack 0 `Current` |
| 3 | `Restart` | conferma → `esp_restart()` | — |
| 4 | `About` | apre AboutScene | — |

Il valore a destra è mostrato solo per la riga 2 (nome pack attivo, `:293`). Footer centrato `xphone-os 0.5.0 (x3|x4)` a `h-44-29-6` (`:300-303`). Picker: paginazione a pagina intera `pageStart = (_pickSel/perPage)*perPage`, dimensioni formattate `731.2 KB` / `1.4 MB` (`:63-74`); stati vuoti "No SD card" e "No .bin files found". Anteprima icone: griglia 2×3, icone ricampionate 104→**72** px, etichette a `:331`.

| Tasto | Menu / Picker | IconStyle | Confirm* |
|---|---|---|---|
| `Left`/`Up` | selezione su | pack precedente (`PREV`) | — |
| `Right`/`Down` | selezione giù | pack successivo (`NEXT`) | — |
| `Confirm` | `OPEN` / apre conferma flash | — | `YES` |
| `Back` | launcher / torna al menu | torna al menu | `NO` |

Soft-key (`:94-101`): liste `BACK/OPEN/UP/DOWN`, conferme `NO/YES/-/-`, icone `BACK/-/PREV/NEXT`.
**Stato persistito**: solo `iconPack`. `onEnter()` resetta vista e selezione (`:88-92`).

---

## About — `scenes/AboutScene.cpp`

Pagina diagnostica: il device non ha cavo seriale in uso normale, quindi About **è** lo strumento di misura. Righe in `kFontRegular` a `x = 24` da `y = 16`, separatori 2 px fra i blocchi (`:33`, `:91`, `:109`, `:179`).

| Blocco | Contenuto |
|---|---|
| Identità | `version` + `__DATE__ __TIME__`; `panel: <BoardConfig::ACTIVE.name> WxH`; `boot to first paint: N ms` (`gBootTotalMs`) |
| Wake | `wake: <esp_reset_reason>  restore: <nome scena>` (`gWakeResetReason`, `gWakeRestoreScene`) |
| Orologio | `time.sync: ble N ms  date N ms  HH:MM` da `CLOCK_STORE`, oppure "date pending" / "no connect since boot" |
| Memoria | heap free, heap min free, largest free block |
| Refresh | `last draw`, `last refresh` + tier (FULL/HALF/FAST/PARTIAL/FLASH), `partials since scrub: n (HALF every 10)` |
| Potenza | `cpu MHz` + uptime min; batteria %/mV/avg mA; su X3 `gauge: rem/fcc mAh design N mAh` (verifica del reprogram a 650 mAh); `adv: off|stopped|fast|slow`; `conn: intervallo/latency/timeout` |
| BLE/ANCS | `ble:` stato, `peer:` indirizzo, `ancs:` messaggio, `notifications stored: N`, `stack HWM: loop/ble + ancs q peak n/6 drops N` |
| Piè | `Press power to sleep (auto N min), hold 3s to restart` |

| Tasto | Azione | Soft-key |
|---|---|---|
| `Back` | launcher | `BACK` (default `Scene::softKeys()`, `Scene.cpp:11-14`) |
| altri | nessuno | — |

Nessuno stato persistito; nessuna azione BLE. I valori sono campionati una volta per render (disciplina e-ink).

---

## Schermata di sleep / poster

La disegna `drawSleepScreen()` in `Sleep.cpp:94-133`, **non** una scena: framebuffer pulito, composizione, `requestResync(1)` e un unico `FULL_REFRESH`, poi il pannello va in deep sleep tenendo l'immagine a ~0 corrente.

Tre facce, selezionate dalla scena che era su vetro:
1. **Workout** (`gCurrentSceneId == SceneId::Workout` e store non vuoto): `WorkoutScene::renderDormant` — titolo "Today's workout", filetto 56×2, righe con checkbox + `done/sets`.
2. **Priorities** (store non vuoto): `PrioritiesScene::renderDormant` — titolo "Today's priorities", `listTop = 100`, `rowH = 48` compresso fino a 38 se la lista è lunga, `+N more` in overflow, limite inferiore `h-176`.
3. **Fallback**: wordmark "xphone" a `h·2/5` + filetto.

Footer condiviso in tutte e tre: riga **blocco** (lucchetto + `Until 10:30 AM | Today: 2`, oppure `Today: 2`) a `h-108`, che slitta a `h-148` se presente anche la riga **calendario** (glifo calendario + primo evento con orario dal Today store, esclusi reminder e "All day"); infine `press power to wake` in `kFontSmall` a `h-56`.

Prima di disegnare, se il link è su, `sleepNow()` chiede uno snapshot fresco di priorities e today con attesa fino a **3500 ms** ciascuno, interrotta appena la revisione cambia (`Sleep.cpp:327-350`).

---

## Riassunto: scena → soft-key → store → card/comandi BLE

| Scena | `SceneId` | Soft-key (slot 0..3) | Store letti | Card consumate | Comandi emessi |
|---|---|---|---|---|---|
| Launcher | 0 | ⚙/`OPEN`/`PREV`/`NEXT` | — (batteria, stato BLE) | — | — (long-press `Down` → `block.start`) |
| Notifications | 1 | `BACK`/`OPEN`\|`SYNC`/`UP`/`DOWN`; dettaglio `BACK`/`CLEAR`/`PREV`/`NEXT` | `NOTIFICATION_STORE` (24) | — (flusso ANCS) | ANCS resync, ANCS dismiss |
| Settings | 2 | `BACK`/`OPEN`/`UP`/`DOWN`; `NO`/`YES`; `BACK`/–/`PREV`/`NEXT` | — | — | — |
| Block | 3 | `BACK`/`START`/`MODE`; active `BACK`/–/`BREAK`; liste `BACK`/`SELECT`/`UP`/`DOWN` | `BLOCK_STATUS` | `block-status` | `block.start`, `block.break`, `block.stop`, `block.status` |
| Priorities | 4 | `BACK`/`DONE`\|`SYNC`/`UP`/`DOWN` | `PRIORITIES_STORE` (10) | `priorities.snapshot` | `priorities.sync.request`, `priority.toggle` |
| Today | 5 | `BACK`/`SYNC`/`UP`/`DOWN` | `TODAY_STORE` (6) | `today.snapshot` | `today.sync.request` |
| About | 6 | `BACK`/–/–/– | `NOTIFICATION_STORE`, `CLOCK_STORE` | — | — |
| Reader | 7 | `BOOKS`/`SIZE`/`PREV`/`NEXT`; scaffale `BACK`/`OPEN`/`UP`/`DOWN` | `ReadingStats` | — | — |
| Workout | 8 | `BACK`/`+SET`\|`SYNC`/`UP`/`DOWN` | `WORKOUT_STORE` (8) | `workout.snapshot` | `workout.sync.request`, `workout.set` |
| File Transfer | 9 | `BACK`/`SYNC`; `CANCEL`; `EXIT`; `BACK`/`RETRY` | — (`WifiCreds` NVS) | — (`transfer.start`/`stop`/`wifi` in ingresso) | notify `transfer.status` |

---

## Cose da sistemare / attriti

1. **L'anteprima "Icon style" abbina le etichette sbagliate alle icone.** `SettingsScene.cpp:331` usa l'ordine `Notif, Read, Today, Priorities, Block, Workout`, ma `IconStyle::iconForApp(i)` indicizza `XPhoneIconPacks` nell'ordine del launcher `Today, Notifications, Priorities, Block, Read, Workout` (`LauncherIcons.h:2274-2276`, `LauncherScene.cpp:22-24`): nella griglia di anteprima l'icona di Today è etichettata "Notif" e quella di Notifications "Read". Gravità **alta** (bug visibile in Settings). Fix: sostituire `kLabels` con l'array condiviso dei nomi app nell'ordine del launcher, esposto da un unico header.
2. **Il launcher smista le app confrontando le stringhe dell'etichetta.** `LauncherScene.cpp:127-140` fa `strcmp(app, "Block")`, `"Read"`, ecc.: rinominare un tile per motivi di UI rompe silenziosamente la navigazione (tile che non apre nulla) e ogni pressione paga 6 `strcmp`. Gravità **media**. Fix: `enum class AppSlot` con `switch` sull'indice e tabella `{label, showFn}` unica.
3. **About sfonda la barra soft-key.** Da `AboutScene.cpp:24-238` le righe accumulano `29+4` px (alcune `+10`) partendo da `y=16`: con la riga `gauge` presente (X3) l'ultima riga cade intorno a `y≈766`, mentre la fascia soft-key inizia a `792-44 = 748` — le ultime due righe finiscono sotto/oltre le tab. Gravità **media** (diagnostica illeggibile proprio dove serve). Fix: paginare About con `UP`/`DOWN` o comprimere i blocchi in due colonne.
4. **`_durationCustomized` non si azzera mai uscendo da Block.** Impostato a `true` in `adjustDuration` (`BlockScene.cpp:189`), viene rimesso a `false` solo da `startDeepWork` (`:233`) o dalla selezione di un preset (`:364`); `onEnter()` (`:161-172`) non lo tocca. Dopo un singolo tap su `+`, l'adozione della durata configurata sul telefono (`:654-656`) resta disabilitata per sempre. Gravità **media**. Fix: azzerarlo in `onEnter()`.
5. **La riga statistiche di Block promette il totale storico e non lo mostra.** Il commento a `BlockScene.cpp:465-467` dice "today's count, the streak … and the all-time total", ma `snprintf` a `:472` stampa solo `Today` e `Streak`; `stats.total` serve solo come guardia. Gravità **bassa**. Fix: aggiungere il totale alla riga o correggere il commento.
6. **`priority.toggle` non ha timeout né stato ottimistico.** `PrioritiesScene.cpp:150-163` invia e attende la nuova snapshot: se il telefono non risponde, la checkbox non cambia e "Updating priority..." resta sullo schermo indefinitamente (nessun analogo di `kTransientTimeoutMs`). Gravità **media** (l'utente non sa se l'azione è passata). Fix: transiente con timeout ~4 s come in BlockScene, che ricade su un messaggio "non confermato".
7. **L'invio coalescato di Workout è indicizzato per posizione, non per id.** `s_pendingIdx` (`WorkoutScene.cpp:24`, `:116`) viene risolto in item solo alla scadenza (`:31`): se nei 400 ms arriva una `workout.snapshot` che riordina o accorcia la lista (`WorkoutStore::updateFromCard` riscrive gli slot), il `workout.set` esce con l'id sbagliato o non esce affatto. Gravità **media**. Fix: memorizzare l'`id` (65 B) invece dell'indice.
8. **Il suggerimento di Workout descrive i tasti sbagliati.** La riga `Press left (-) and right (+) to count sets.` (`WorkoutScene.cpp:224`) non corrisponde alla mappatura reale: i conteggi sono su `Up`/`Down` del bordo superiore (`:153-160`), mentre `Left`/`Right` frontali muovono la selezione (`:163-170`). Gravità **media** (l'unico affordance dei tasti laterali è fuorviante). Fix: riformulare in "tasti superiori − / +".
9. **La quick-action Deep Work è invisibile.** Il long-press di `Btn::Down` sul launcher (`LauncherScene.cpp:118-121`) sta su un tasto del bordo superiore, che non ha tab soft-key: `longPressSlots()` può marcare solo i 4 slot frontali (`Scene.h:38-44`), quindi nessun puntino può segnalarla. Gravità **bassa**. Fix: spostarla su uno slot frontale oppure disegnare un hint accanto al tile Block.
10. **`rowRect()` duplica a mano l'altezza di riga delle notifiche.** `NotificationsScene.cpp:100` scrive `29 + 29 + 14` con un commento che dice "matches rowHeight()" invece di chiamare `rowHeight(gfx)`: cambiando font i rettangoli di refresh parziale puntano righe sbagliate (fantasmi e artefatti, non un crash). Gravità **bassa**. Fix: passare `Gfx&` e riusare `rowHeight()`.
11. **`today.sync.request` non ha controparte iOS.** La soft-key `SYNC` di Today è sempre attiva (`TodayScene.cpp:163-167`) ma il comando è dichiarato senza handler nell'app companion (`ble/CompanionBleService.h:118-122`) e nell'header della scena (`TodayScene.h:12-16`): premerlo non produce nulla. Gravità **media** per chi riscrive l'app iOS: è un requisito da implementare, non un bug del firmware.
12. **Commenti dell'arte e del launcher fuori sincrono con il codice.** `art/LauncherIcons.h:6-7` dichiara l'ordine app `Notifications, Read, Today, Priorities, Block, Workout` (smentito dalla tabella a `:2273-2276`); `LauncherScene.cpp:55` parla di "5 tiles" con `APP_COUNT = 6`; `LauncherScene.h:3` dice "3x2 app grid" mentre `COLS = 2` (`LauncherScene.h:16`) produce 2 colonne × 3 righe. Gravità **bassa**, ma è esattamente la documentazione su cui si basa un fork. Fix: rigenerare l'header con l'ordine reale e correggere i due commenti.
13. **Codice morto minore.** `IconStyle.cpp:43-45` contiene un `if` con corpo vuoto; `TodayScene.cpp:333` ha un `(void)smallH` per una variabile calcolata e mai usata (`:288`); i default `_localMsg` di Priorities/Today (`PrioritiesScene.h:65`, `TodayScene.h:59`) sono sovrascritti da `onEnter()` prima di ogni render. Gravità **bassa**. Fix: rimuovere.
14. **Le coordinate del footer dormant sono costanti assolute.** `h-108`, `h-148`, `h-176`, `h-56` compaiono ripetute in `Sleep.cpp:101-123` e `PrioritiesScene.cpp:352-404`/`WorkoutScene.cpp:316-317`: un pannello con altezza diversa da 792/800 (o un font più grande) fa collidere la lista con il footer senza che nulla se ne accorga. Gravità **bassa**. Fix: costanti condivise derivate da `gfx.height()` in un unico header di layout dormant.
