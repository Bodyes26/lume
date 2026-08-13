# Personalizzazione e localizzazione (fork X3-only)

Analisi di fattibilità su `flowe-os/xphone-os/` (repo read-only). Ogni affermazione è ancorata a `file:riga`. Marcatura: fatti verificati senza prefisso, deduzioni con `[INFERENZA]`. Fuori scope: protocollo BLE e OTA.

Contesto di piattaforma rilevante per tutti i punti:
- Coordinate logiche **portrait** 528x792 su X3; `Gfx::begin()` scambia larghezza/altezza del pannello nativo (`src/Gfx.cpp:6-14`) e `Gfx::drawPixel()` ruota 90° CW verso il framebuffer nativo landscape (`src/Gfx.cpp:16-32`).
- Render **solo su dirty flag**, nessun redraw periodico (`src/Scene.h:5-11`, `src/main.cpp:653`); scene statiche, nessuna heap (`src/Scene.h:11`).
- Partizione app 6.5 MB (`xphone-os/partitions.csv`: `app0 0x640000`); l'immagine attuale usa ~2.5 MB e ~146 KB di RAM (`README.md:70-72`) → ~4 MB di flash liberi.

---

## 1. Localizzazione italiana con switcher runtime

### Dove vivono le stringhe (censimento eseguito su tutti i `.cpp/.h` di `src/`)

Metodo: estrazione dei letterali stringa per riga, esclusi commenti/`#include`, con classificazione per contesto (log `Serial.*`, chiavi NVS/JSON, path, testo UI). Conteggio delle **occorrenze UI** (letterali che finiscono su schermo, incluse le format string):

| File | Occorrenze UI | Note |
|---|---|---|
| `src/scenes/BlockScene.cpp` | 52 | include preset+azioni (`:26-29`, `:38-40`), messaggi di stato, `"min left"`/`"min break"` (`:526`) |
| `src/scenes/SettingsScene.cpp` | 39 | voci menu (`:38`), label pack icone (`:331`), conferme (`:402-419`) |
| `src/scenes/AboutScene.cpp` | 37 | quasi tutte `snprintf` diagnostiche (`:35-232`) |
| `src/scenes/ReaderScene.cpp` | 34 | stati Opening/Indexing/Error (`:842-911`), statistiche (`:987-998`) |
| `src/scenes/PrioritiesScene.cpp` | 26 | include frame dormiente (`:343-404`, `:417-423`) |
| `src/scenes/FileTransferScene.cpp` | 25 | testo spezzato a mano su più righe (`:271-323`) |
| `src/scenes/NotificationsScene.cpp` | 22 | `"Notifications (%d)"`, `"%d of %d"` |
| `src/scenes/WorkoutScene.cpp` | 22 | `"%d / %d done"`, `"%d sets"` |
| `src/scenes/TodayScene.cpp` | 18 | header `"TODAY"/"TONIGHT"/"REMINDERS"` (`:297-311`) |
| `src/scenes/LauncherScene.cpp` | 17 | nomi app (`:22-24`), wordmark `"flowe"` (`:173`) |
| `src/scenes/AppScenes.cpp` | 11 | nomi scena per diagnostica/titoli (`:128-138`) |
| `src/Scene.cpp` | 8 | soft-key default (`:12`), tier refresh |
| `src/main.cpp` | 6 | splash `"flowe"`/`"waking up..."` (`:91-95`), `"Restarting..."` (`:552`) |
| `src/Sleep.cpp` | 2 | `"press power to wake"`, `"xphone"` (`:104-123`) |
| `src/SyncIndicator.h`, `src/StatusBar.h`, `src/BatteryGauge.cpp`, `src/SdUpdate.cpp` | 6 | `"Sent"/"Received"` (`SyncIndicator.h:39-40`), `"%d%%"/"--%%"` (`StatusBar.h:82-84`) |
| **Totale scene/UI** | **323 occorrenze → 213 stringhe uniche** | 51 delle uniche contengono format `printf` |

A queste si aggiunge il rail di stato BLE, anch'esso mostrato dalle scene:
- 38 letterali `setStatus("…")`, 36 unici (`src/ble/CompanionAncsClient.cpp:544-1565`, `src/ble/CompanionBleService.cpp:627-847`) — include un bug di branding: `"Pair X4 in iPhone Bluetooth"` (`CompanionAncsClient.cpp:585`) su un X3.
- 14 `statusMessage = "…"` unici (`CompanionBleService.cpp:386-1187`).

**Totale realistico da tradurre: ~263 stringhe uniche**, di cui ~60 sono format string con placeholder.

### Soft-key (vincolo forte)

27 array statici `static constexpr const char* kX[4]` restituiti da `softKeys()`, 22 label uniche: `BACK, OPEN, PREV, NEXT, UP, DOWN, SYNC, DONE, SELECT, START, MODE, BREAK, BOOKS, SIZE, CLEAR, CANCEL, EXIT, RETRY, SETTINGS, YES, NO, +SET` (`BlockScene.cpp:175-180`, `FileTransferScene.cpp:60-64`, `LauncherScene.cpp:150`, `NotificationsScene.cpp:68-71`, `PrioritiesScene.cpp:140-141`, `ReaderScene.cpp:263-267`, `SettingsScene.cpp:95-97`, `TodayScene.cpp:164-165`, `WorkoutScene.cpp:101-102`, `Scene.cpp:12`).

Il contratto richiede **stringhe statiche lette a render time** (`Scene.h:29-30`); `drawSoftKeyBar` le legge una volta per repaint (`Scene.cpp:97-106`, chiamato da `Scene.cpp:181`). Restituire puntatori a letterali in flash di un'altra lingua rispetta il contratto: i puntatori restano validi per sempre.

Larghezza tab su X3: `marginX = 528*8/100 = 42`, `tabW = (528 - 84 - 30)/4 = 103 px` (`Scene.cpp:28-29`, `:56-67`). Misure calcolate con le advance reali di `ubuntu_10_regular_ascii.h` (12.4 fp, `Gfx.cpp:239-249`):

| IT | px | Esito | IT | px | Esito |
|---|---|---|---|---|---|
| INDIETRO | 94 | ok (stretto) | CONFERMA | **112** | **overflow** |
| ANNULLA | 94 | ok (stretto) | IMPOST. | 81 | ok |
| AVANTI | 74 | ok | PRECED. | 83 | ok |
| RIPROVA | 88 | ok | SINCR. | 63 | ok |
| FATTO | 65 | ok | AVVIA | 61 | ok |

→ serve una regola: label soft-key IT max ~95 px (≈8 caratteri maiuscoli), `CONFERMA` va abbreviato in `OK`/`SCEGLI`.

### Copertura glifi (verificata, non assunta)

I font UI **non** sono ASCII-only: `tools/subset_epd_font.py:42` emette tre intervalli e gli header li confermano — `{0x20,0x7E} {0xA0,0xFF} {0x100,0x17F}`, 319 glifi (`src/fonts/ubuntu_12_regular_ascii.h:911-915`, `ubuntu_12_bold_ascii.h:968-972`, `ubuntu_10_regular_ascii.h:753-757`).
- Accenti italiani presenti con bitmap reali: `à U+00E0 (11x20)`, `è U+00E8`, `é U+00E9`, `ì U+00EC`, `ò U+00F2`, `ù U+00F9`, `À U+00C0 (17x24)`, `È U+00C8` (`ubuntu_12_regular_ascii.h`, righe glifo con `dataLength > 0`).
- `° U+00B0` presente in tutti e tre i font (regular 8x7, bold 9x7, small 7x6).
- `· U+00B7` presente → il workaround "middle dot stampato come quadrato 3x3" e il commento "UI fonts are ASCII-only subsets" in `ReaderScene.cpp:928-929` sono **obsoleti**.
- Assente: `€ U+20AC` (fuori dai tre intervalli). Se serve, va rigenerato l'header (vedi §3).
- Il decoder UTF-8 di `Gfx::nextCodepoint` gestisce già i multibyte (`Gfx.cpp:152-185`), fallback `'?'` (`Gfx.cpp:245`, `:257`).

### Meccanismo proposto

```c
// src/i18n.h — nessuna allocazione, tutto in .rodata
enum Str : uint16_t { STR_BACK, STR_SYNC, ..., STR_COUNT };
enum Lang : uint8_t { LANG_EN, LANG_IT, LANG_COUNT };
extern const char* const kStr[LANG_COUNT][STR_COUNT];   // ~263 slot
inline const char* T(Str s);                            // legge gLang (uint8_t in BSS)
```
- Lingua persistita in NVS namespace `"xphone"` con chiave `"lang"` (≤15 char, come le altre: `Sleep.cpp:44-70`, `IconStyle.cpp:9-10`); caricamento lazy con il pattern `ensureLoaded()` di `IconStyle.cpp:21-29`.
- `softKeys()`: sostituire ogni `static constexpr const char* kX[4]` con `static const char* buf[4]; buf[0]=T(STR_BACK); …; return buf;` — sicuro perché `softKeys()` è invocato solo dal main loop (`Scene.cpp:181`) e i puntatori puntano a flash.
- Switch runtime: nuova voce in `SettingsScene` (`:38` per la lista, `:311-316` come modello per il sotto-view a `PREV/NEXT`), poi `IconStyle`-style `set()` + `markDirty()`; il cambio è full-panel, quindi `markDirty()` senza rect.
- Costo flash misurato/stimato: stringhe EN attuali 3.668 B (213 uniche, media 17,2 char); versione IT ≈ 4.400 B (+20%); due tabelle di puntatori 213×4 B = 852 B ciascuna → **≈ 6,1 KB** aggiunti, su ~4 MB liberi: irrilevante.
- Nessun impatto su heap/loop: solo dereference di puntatori costanti.

### Punti dove la lingua non basta

| Punto | Evidenza | Problema |
|---|---|---|
| Ora/data | `BlockStatusStore.h:28-30`, `TodayStore.h:33`, `:51`, `CompanionProtocol.h:106-108` | `endsAtLabel`, `Item::time`, `syncLine` arrivano **già formattati dall'iPhone** in 12h US (`"10:30 AM"`, `"Synced 9:41 AM"`): il device non può localizzarli; va cambiata l'app iOS |
| Confronto semantico su stringa inglese | `PrioritiesScene.cpp:449` `strcmp(item.time,"All day")`, `:448` e `TodayScene.cpp:127/139/317` `strcmp(item.kind,"reminder")` | se il telefono localizza, il device rompe il filtro → serve un flag booleano nel protocollo |
| Direzione transfer | `SyncIndicator.h:33-40` classifica per suffisso `" sent"` / `" received"` sui messaggi BLE | localizzare `statusMessage` rompe le frecce → separare `direction` dal testo |
| Plurali | `SettingsScene.cpp:361` `"%d file%s"`, `FileTransferScene.cpp:314-315` `"%u request%s"` | l'hack `"s"`/`""` non funziona in italiano (file→file, richiesta→richieste): serve una forma per numero |
| Orologio 24h | `AboutScene.cpp:58` `"%02u:%02u"` da `CLOCK_STORE.minutesIntoDay` (`ClockStore.h:20`) | l'unico orario formato sul device è già 24h; nessun RTC (`Sleep.cpp:89-90`) |
| Giorni settimana | `ReaderScene.cpp:1013` `"MTWTFSS"` | va sostituito con `"LMMGVSD"` |
| Rail BLE | 36+14 stringhe in `src/ble/` | testo tecnico mostrato all'utente; include `"Pair X4 …"` su X3 |

**Complessità: media.** File da toccare: nuovi `src/i18n.{h,cpp}`, poi tutte le 11 scene, `Scene.cpp`, `Sleep.cpp`, `main.cpp`, `StatusBar.h`, `SyncIndicator.h`, `SettingsScene.cpp` (switcher), `src/ble/*.cpp` (rail). Rischi: overflow layout (soft-key, `truncateToWidth` già presente per le liste, es. `PrioritiesScene.cpp:374`); regressione delle frecce sync; stringhe generate dal telefono restano inglesi finché non si cambia l'app.

---

## 2. Lettura in orizzontale (landscape)

### Come è implementata oggi la rotazione

Punto di trasformazione unico: `Gfx::drawPixel` (`Gfx.cpp:16-32`) — `phyX = y; phyY = _w-1-x`, cioè 90° CW, stessa chiralità di CrossPoint Portrait (`Gfx.h:16-23`). `begin()` fissa `_w = getDisplayHeight()`, `_h = getDisplayWidth()` (`Gfx.cpp:8-12`). Tutto il resto (rect, glifi, blit 1bpp) passa da lì.

### Cosa assume portrait

| Punto | Evidenza |
|---|---|
| Mapping logico↔nativo fisso, nessun parametro di rotazione | `Gfx.cpp:8-12`, `:23-24` |
| Finestra partial: span nativo X = span logico **Y**, allineato a 8 px (UC8253 PTL) | `Gfx.cpp:365-378`, idem `flushWindowFlash` `:409-417` |
| Barra soft-key ancorata in basso, 44 px riservati, margine 8% | `Scene.h:21`, `Scene.cpp:22-29`, `:56-67` |
| Viewport reader = `w-2*24` x `h-24-24-44` | `ReaderScene.cpp:80-82`, `:829-830` |
| Origine testo pagina `(kMarginX, kMarginTop)` | `ReaderScene.cpp:917` |
| Riga di stato reader agganciata a `height()-SOFTKEY_BAR_H-kStatusH` | `ReaderScene.cpp:942` |
| Frame dormiente: costanti `h-108`, `h-148`, `h-176`, `h-56` | `Sleep.cpp:101-123`, `PrioritiesScene.cpp:352-404` |
| Griglia libri 2x2 + thumb 200x260, cache `cover_<w>x<h>.bin` | `ReaderScene.cpp:91-99`, `CoverThumb.h:1-17` |
| Mapping pulsanti fisso, non rimappato (i 4 frontali stanno sul lato corto tenuto in basso in portrait) | `Input.h:6-12`, `Gfx.h:19-22` |

### Fattibilità

**Fattibile come modalità per-scena, ma non gratis.** Elementi favorevoli:
- La misura/impaginazione è **resolution-agnostic**: il viewport è un parametro (`ReaderSettings.h:14-17`, `:26-27`) preso dai `gfx.width()/height()` correnti (`ReaderScene.cpp:827-830`).
- La cache `section.bin` ha `viewportWidth/Height` **nell'header** e li confronta ad ogni load (`Section.cpp:61-67`, `:109-119`, versione 28 a `:28`): cambiando orientamento le cache vengono invalidate e ricostruite **automaticamente**, senza bookkeeping extra. Costo: re-indicizzazione del capitolo (frame "Indexing chapter...", `ReaderScene.cpp:844-848`).
- La misura del testo non dipende dall'orientamento, solo dalla larghezza riga (`TextMeasure.h:19-32`, `ParsedText.cpp` line-break DP).

Cosa serve:
1. Parametrizzare la rotazione in `Gfx`: campo `_rot` + `setRotation()`, con `drawPixel` a due rami e `_w/_h` ricalcolati; aggiornare `flushWindow`/`flushWindowFlash` (in landscape l'allineamento a 8 px cade sulla X **logica**, non sulla Y).
2. Barra soft-key: in landscape i 4 tasti fisici finiscono sul lato **corto verticale** → o si ruota la barra sul bordo destro/sinistro, o si accetta l'incoerenza tasto↔tab. Nessun rimapping input esiste (`Input.h:9-12`), quindi le direzioni Up/Down andrebbero reinterpretate.
3. Ricalcolare le coordinate ancorate al bordo inferiore (elenco sopra): sono ~12 siti.
4. Griglia libri/cover: nome file cache già parametrico su `w x h` (`CoverThumb.h:30-42`), ma i valori 200x260 e `kGridCols/Rows` sono costanti → in landscape servono 3x1 o 2x2 con box diverso, e nuovi thumb (nuovo file, il vecchio resta).
5. Escludere le altre scene: lo stato `Reading` ruota, `BookList`/launcher/sleep restano portrait → richiede rotazione + `markDirty()` full su ogni transizione di stato dentro `ReaderScene`.

**Complessità: alta.** File: `Gfx.{h,cpp}`, `Scene.{h,cpp}`, `ReaderScene.cpp`, `Input.h`, `Sleep.cpp`, `PrioritiesScene.cpp`, `CoverThumb`/griglia. Rischi: partial-refresh disallineato (silent no-op o finestra sbagliata: `Gfx.cpp:365-370`), re-indicizzazione di tutti i capitoli al primo switch, incoerenza tasti/etichette.

---

## 3. Più font per il reader

### Formato attuale

`EpdFontCore` usa font **2 bpp compressi a gruppi DEFLATE**: header generati da `fontconvert.py … --2bit --compress --pnum` (intestazione di `lib/EpdFontCore/builtinFonts/notoserif_12_regular.h:1-7`). Strutture: `EpdGlyph` (16 B con padding: `width,height,advanceX 12.4,left,top,dataLength,dataOffset`), `EpdFontGroup{compressedOffset,compressedSize,uncompressedSize,glyphCount,firstGlyphIndex}`, `EpdUnicodeInterval`, tabelle kern per classi + legature, e un hook `glyphMissHandler/glyphMissCtx` per font "non residenti (es. SD card)" (`lib/EpdFontCore/EpdFontData.h:70-144`).

Numeri reali (misurati sugli array degli header, 12 file):

| Font | Bitmap compresso | Tabella glifi | Kern+altro | Totale |
|---|---|---|---|---|
| notoserif_12_regular | 26.504 B | 1.071×16 = 17.136 B | 6.425 B | **50.065 B** |
| notoserif_14_regular | 32.902 B | 17.136 B | 6.425 B | **56.463 B** |
| notoserif_16_regular | 38.333 B | 17.136 B | 6.495 B | **61.964 B** |
| notoserif_16_bolditalic | 47.850 B | 17.120 B | 13.335 B | **78.305 B** |

- Una **famiglia completa a una misura** (R/B/I/BI): 12pt 226.691 B, 14pt 254.780 B, 16pt 279.858 B.
- I 12 header attualmente linkati (`src/reader/ReaderFonts.cpp:7-39`): **761.329 B ≈ 744 KB** di flash.
- 1.071 glifi, 20 intervalli, 13 gruppi per font; copertura: ASCII, Latin-1, Latin-Ext-A, parte di Latin-Ext-B, combining U+0300-036F, Cirillico completo, U+1EA0-1EF9 (vietnamita), punteggiatura generale, apici/pedici, valute, legature `U+FB00-FB06`, `U+FFFD` (letto da `notoserif_12_regularIntervals`). Per l'italiano è già sovrabbondante.
- Il generatore `fontconvert.py` **non è nella repo** (nessun file in `xphone-os/tools/`, che contiene solo `lto_link.py`, `patch_ble_service_friend.py`, `subset_epd_font.py`).

### RAM del decompressore

`FontDecompressor` inflate un gruppo intero: `hotGroup.resize(group.uncompressedSize)` (`FontDecompressor.cpp:177`) e, in prewarm, `malloc(group.uncompressedSize)` per il temp (`:465`). Dimensioni gruppo reali su `notoserif_16_regular`: max **38.354 B** (256 glifi), tipico 10-19 KB; su 14pt max 29.918 B. Prewarm: max 4 slot (uno per stile, `FontDecompressor.h:11-12`) e 512 glifi per pagina; gli slot vengono liberati a fine `renderPage` (`BookTextRenderer.h:15-20`).

### Vie realistiche per aggiungere un font

1. **Rigenerare header con tool proprio (raccomandata).** Serve reimplementare `fontconvert.py`: rasterizzare TTF (FreeType/PIL), quantizzare a 2 bpp, raggruppare i glifi in blocchi, comprimere con `zlib.compress(..., wbits=-15)` (raw DEFLATE, compatibile con uzlib: `FontDecompressor.cpp:60-67` usa `inflateReader.init(false)` su buffer contiguo), emettere `Bitmaps/Glyphs/Intervals/Groups` + `EpdFontData`. Kern/legature possono essere `nullptr`/0 (campi opzionali per contratto, `EpdFontData.h:120-131`). Costo: **+50÷80 KB di flash per stile-misura**; con ~4 MB liberi si possono aggiungere comodamente 2-3 famiglie complete a 3 misure. Complessità: **media** (il tool è il lavoro vero; il firmware cambia solo in `ReaderFonts.{h,cpp}` — `kReaderFontCount` a `ReaderFonts.h:8` e lo switch a `:43-53`, più `ReaderScene::cycleFontSize` `:645-656` e la chiave NVS `rdFont` `:76`).
   Attenzione: `fontId` è **la chiave di cache** di `section.bin` (`ReaderSettings.h:19-21`, `Section.cpp:62`): rimappare gli id invalida tutte le cache esistenti (rebuild silenzioso, non corruzione).
2. **Caricare font da SD a runtime.** Il formato lo prevede (`glyphMissHandler`, `EpdFontData.h:133-143`) ma **non esiste nessuna implementazione** nella repo (le uniche referenze sono in `EpdFont.cpp:159`, `:180-181`). Ostacoli concreti: `decompressGroup` prende `&fontData->bitmap[group.compressedOffset]`, cioè richiede lo stream **contiguo in memoria** (`FontDecompressor.cpp:66`); servirebbe una variante che legga dal file (InflateReader ha una modalità streaming con callback e finestra 32 KB — `lib/InflateReader/InflateReader.h:16-37`, `:40`, ma quella finestra è già contesa: il reader se la prende in prestito dal framebuffer, `:44-62`). Inoltre la tabella glifi (17 KB) e gli intervalli dovrebbero stare in RAM o essere letti per-glifo. Su 320 KB totali con ~146 KB già usati: **complessità alta, beneficio basso** rispetto all'opzione 1.

### Font UI

`tools/subset_epd_font.py` **non genera** font: fa subsetting di header `fontconvert.py` non compressi presi da `../x4-os/lib/EpdFont/builtinFonts/…` (docstring `:26-32`) — path **inesistente** in questo repo (`flowe-os/` contiene solo `xphone-os`, `freeink-sdk`, `docs`). Quindi: per cambiare font UI o aggiungere `€`/altri range serve prima un `fontconvert` proprio in modalità **non compressa** (`Gfx` legge bitmap 1 bpp MSB-first direttamente, `Gfx.h:10-14`, `Gfx.cpp:198-237`), poi si riusa `subset_epd_font.py` (che ha già la self-verification C-faithful, `:13-24`) cambiando `RANGES` a `:42`. `XpFont` porta solo bitmap/glifi/intervalli + `lineAdvance`/`ascender` cablati a mano (`Fonts.cpp:20-47`, `Gfx.h:50-57`): un font nuovo richiede di leggere quei due valori dal generatore. Costo attuale font UI: 6.666 + 9.193 + 10.112 B di bitmap + 3×319×16 B di glifi ≈ **41 KB**.

---

## 4. Sleep/blocco, icona di sync, icone launcher

### Come sono fatti oggi

| Elemento | Codice | Formato / dati |
|---|---|---|
| Schermata sleep | `Sleep.cpp:94-133` | 3 varianti: dormiente Workout (`WorkoutScene::renderDormant`), dormiente Priorities (`PrioritiesScene.cpp:333-406`), fallback wordmark `"xphone"` + rule 56x2. Footer comune: block line (`PrioritiesScene.cpp:411-434`) + evento calendario (`:440-470`) + `"press power to wake"`. Chiude con `requestResync(1)` + `FULL_REFRESH` (`Sleep.cpp:131-132`) |
| Icone launcher | `art/LauncherIcons.h:1-20`, `IconStyle.cpp:54-58` | 104x104, 1 bpp MSB-first, **bit a 0 = ink**, 13 B/riga → **1.352 B/icona**; 5 pack × 6 app = 30 array = **40.560 B**; indicizzate `XPhoneIconPacks[pack][app]` (`:2274`); pack attivo in NVS `"xphone"/"iconPack"` (`IconStyle.cpp:9-10`), clamp a `XPhoneIconPackCount` |
| Blit icone | `LauncherScene.cpp:34-45` | nearest-neighbor con stride fisso 13 B (bug storico documentato a `:30-33`) |
| Artwork Block | `art/BlockArtwork.h:17-22`, `:1849-1852` | X3 hero 480x330 = **19.800 B**, work 340x340 = 14.620 B, 3 icone 48x48 = 288 B ciascuna; ramo `#if defined(FREEINK_DEVICE_X3)` (`:20`, `#else` a `:1848`) |
| Logo boot | `art/FloweLogo.h:9-12` | 120x120 = **1.800 B**, stesso formato; blit in `main.cpp:76-84`, splash `main.cpp:86-97` |
| "Icona di caricamento" | non esiste uno spinner | 3 affordance distinte: frecce transfer su/giù (`SyncIndicator.h:87-116`, bold 1 s poi thin), dot sync nella status bar pieno/vuoto (`StatusBar.h:92-111`), e testo (`"Syncing..."`, `"Opening book..."`, `"Indexing chapter..."`, `"SYNCING"`). Il flash-update ha una progress bar font-free a 25% di passo (`SdUpdate.cpp:87-132`) e una X d'errore (`:134-153`) |
| Asset 1 bpp da SD (precedente esistente) | `CoverThumb.h:1-17`, `:59-62` | file `cover_<w>x<h>.bin`: magic `0x5854 'XT'`, versione 1, w/h u16, righe `ceil(w/8)` B MSB-first, **bit a 1 = ink** (opposto delle icone); `draw()` streama una riga alla volta su buffer di stack, **zero heap**, sicuro in render |

### Cosa serve

- **Scegliere tra più layout di sleep**: `drawSleepScreen` è già una catena di fallback su `gCurrentSceneId` (`Sleep.cpp:100-105`). Aggiungere una chiave NVS `"sleepFace"` (namespace `"xphone"`, pattern `IconStyle.cpp:21-52`) e uno switch su N funzioni di render. **Complessità bassa.** File: `Sleep.cpp`, `SettingsScene.cpp` (voce), nuovo modulo `SleepFaces`.
- **Contenuti diversi durante il blocco**: i dati sono già persistiti a sleep-time (`Sleep.cpp:213-303`: block, today, priorities, workout, notifiche) e ri-seedati a boot (`:438-495`); un poster "blocco" può leggere `BLOCK_STATUS`/`TODAY_STORE` senza nuovo I/O. Vincolo: nessun RTC (`Sleep.cpp:89-90`), quindi niente orologio né "aggiornato N min fa"; solo etichette assolute fornite dal telefono. **Complessità bassa/media.**
- **Sostituire l'icona di caricamento**: le frecce sono primitive `drawLine` parametriche (`SyncIndicator.h:87-96`); per un asset bitmap servirebbe un array 1 bpp + blitter (già disponibile come pattern in `LauncherScene.cpp:34-45`). Attenzione: nessuna animazione è ammessa senza refresh periodico — il dot è progettato esplicitamente per cambiare solo sui repaint che il sync già provoca (`StatusBar.h:92-100`). **Complessità bassa.**
- **Asset da SD invece che da flash**: fattibile riusando esattamente il formato/streaming di `CoverThumb::draw` (`CoverThumb.h:59-62`). Costo RAM: un buffer di riga = `ceil(w/8)` B (13 B per un'icona 104px, 60 B per l'hero 480px) → **trascurabile**, nessuna heap. Costi reali: apertura file SD in path di render (l'SD è già montato, `SdMan`) e gestione dell'assenza del file (fallback a flash). Nota: bit-order invertito tra i due formati → normalizzare su uno.
- **Tool mancanti da reimplementare** (citati nei commenti, assenti dalla repo):
  - `tools/xphone-icons/build_launcher_icons.py` (`LauncherIcons.h:4`) — input: PNG per app/pack → 104x104 1 bpp bit0=ink.
  - `tools/x4-block-assets/build_block_artwork.py` + `tools/x4-screen-lab/assets/block/heroes/*.png` (`BlockArtwork.h:6-11`) — pipeline ImageMagick documentata: white-flatten, resize, gray, `-level 20%,95%`, dither ordinato `o8x8`, 1 bpp.
  - `fontconvert.py` (§3).
  - `../x4-os/lib/EpdFont/builtinFonts/*` come input di `subset_epd_font.py`.
  Ricostruirli è meccanico: il formato d'ingresso (PNG/TTF) e quello d'uscita (array C 1 bpp / header EpdFont) sono entrambi documentati nei commenti. **Complessità bassa** per icone/artwork, **media** per fontconvert.

---

## 5. Dashboard da scrivania

### Cosa esiste

- **Nessun refresh periodico**: il loop gira a 10 ms e ridisegna solo su dirty (`main.cpp:644-653`, `Scene.h:5-11`).
- **Auto-sleep**: 10 min di default, 2 min con Block attivo (`Sleep.h:23-34`), timer resettato da qualsiasi pressione (`main.cpp:588-609`); File Transfer pinna la scadenza (`:598`).
- **Poster di sleep** con `FULL_REFRESH` + un passo di ricondizionamento (`Sleep.cpp:131-132`): l'immagine resta sul vetro a ~0 corrente (`Sleep.cpp:82-84`).
- **Disciplina refresh**: FAST per switch/interazioni, PARTIAL per rect ≤50% del pannello, HALF di scrub ogni 10 refresh differenziali, 2 FULL di conditioning a boot (`Scene.cpp:136-216`, `Scene.h:96-99`).
- Il controller pannello resta alimentato per tutta la veglia; l'unico `deepSleep()` è in `Sleep::sleepNow` e il wake è un **reboot completo** (`Scene.cpp:165-169`, `Sleep.h:5-12`).
- **Nessun wake da timer** in tutta la repo: solo `esp_deep_sleep_enable_gpio_wakeup` sul tasto power (`Sleep.cpp:412-413`); ricerca di `esp_sleep_enable_timer_wakeup` → 0 occorrenze.

### Cosa servirebbe

Due strade, con un vincolo hardware decisivo:

1. **Rimanere svegli con refresh a bassa frequenza (unica strada su batteria).** Serve: (a) esenzione dall'auto-sleep per la scena dashboard, sulla falsariga di `FileTransfer` (`main.cpp:598`) o con `XP_AUTO_SLEEP_MS` a 0 (`Sleep.h:23-25`); (b) un tick temporale che chiami `markDirty(rect)` a bassa cadenza (es. 1-5 min) — oggi non esiste alcun timer di render; (c) far cadere il refresh su finestre parziali (`Scene.cpp:184-186`, ≤50% area) e affidarsi allo scrub HALF ogni 10 (`Scene.h:99`) per il ghosting. Punti da modificare: `main.cpp:588-609` (finestra idle), `main.cpp:644-653` (tick dashboard), `Scene.cpp:136-216` (eventuale politica di tier dedicata), nuova scena + voce in `SettingsScene`/`LauncherScene`.
2. **Wake periodico da deep sleep: NON praticabile su batteria.** Prima di dormire il firmware porta GPIO13 (latch MOSFET batteria) a 0 e lo tiene (`Sleep.cpp:394-404`): "on battery the MCU is then completely powered off and the power button hard-wires a power-up". Un `esp_sleep_enable_timer_wakeup` funzionerebbe **solo alimentati via USB** (`Sleep.cpp:406-411` conferma che su USB il GPIO è l'unica via di wake). Inoltre ogni wake è un reboot completo con splash FULL (`main.cpp:86-97`) e ricostruzione di tutti gli store RAM (`CompanionSync.h:5-9`): un ciclo al minuto significherebbe uno flash FULL al minuto — visivamente e per vita pannello inaccettabile. [INFERENZA] Un compromesso sensato è dashboard **solo su USB**, sveglia continua, contenuto rinfrescato ogni 1-5 minuti in finestra parziale.

### Vincoli

- **Consumo**: nessun dato misurato nella repo per la veglia continua; noto invece il motivo per cui la veglia è stata tagliata: pinnare il device sveglio per un blocco da ~2 h scaricava la cella da 650 mAh (`main.cpp:576-582`, `Sleep.h:27-31`). Su USB il problema non esiste; su batteria la dashboard va considerata non supportata. [INFERENZA]
- **Ghosting/LUT**: i refresh differenziali (FAST/PARTIAL) accumulano ghosting per costruzione — nulla fuori dalla finestra viene ripilotato (`Scene.cpp:143-145`); la cadenza di scrub 10 è già più conservativa del default CrossPoint 15 (`Scene.cpp:139-145`). Una dashboard con 1 update/min raggiunge lo scrub HALF ogni ~10 min: accettabile.
- **Vita del pannello**: [INFERENZA] nessun datasheet nella repo; il rischio reale è l'immagine quasi-statica con aggiornamenti parziali ripetuti sulla stessa area (es. un orologio) → mitigare alternando la posizione o forzando un FULL periodico.
- **Orologio**: senza RTC e con l'ora fornita dal telefono via card `time.sync` (`ClockStore.h:1-21`), una dashboard con orologio dipende dalla connessione BLE attiva; `minutesIntoDay` è uno snapshot al momento della ricezione, non un contatore.

**Complessità: media** (strada 1), **non fattibile su batteria** (strada 2).

---

## Riepilogo

| Feature | Complessità | File principali | Prerequisiti |
|---|---|---|---|
| i18n IT + switcher runtime | media | nuovi `src/i18n.{h,cpp}`; `src/scenes/*.cpp` (11), `Scene.cpp`, `Sleep.cpp`, `main.cpp`, `StatusBar.h`, `SyncIndicator.h`, `src/ble/*.cpp` | glossario ≤95 px per soft-key; disaccoppiare `SyncIndicator::classify` dal testo; flag booleani al posto di `strcmp("All day"/"reminder")`; app iOS che invii orari localizzati |
| Font UI con nuovi range (€, altri) | media | nuovo `fontconvert` non compresso; `tools/subset_epd_font.py:42`; `src/fonts/*`, `Fonts.cpp` | tool di generazione (assente in repo) |
| Font reader aggiuntivi (flash) | media | nuovo `fontconvert` 2bpp+DEFLATE; `reader/ReaderFonts.{h,cpp}`, `ReaderScene.cpp:645-656`, `:76` | ~50-80 KB flash per stile-misura; consapevolezza che `fontId` è chiave cache `section.bin` |
| Font reader da SD a runtime | alta | `FontDecompressor.{h,cpp}`, `EpdFont.cpp`, nuovo `SdCardFont` | sorgente inflate streaming da file; budget RAM (max gruppo 38 KB + tabella glifi 17 KB) |
| Layout sleep multipli / poster blocco | bassa | `Sleep.cpp:94-133`, nuovo `SleepFaces`, `SettingsScene.cpp` | chiave NVS `sleepFace`; nessun orologio (no RTC) |
| Icona sync/caricamento personalizzata | bassa | `SyncIndicator.h:87-116`, `StatusBar.h:92-111` | nessuna animazione senza refresh periodico |
| Icone launcher / artwork da SD | bassa/media | `IconStyle.{h,cpp}`, `LauncherScene.cpp:34-45`, riuso `CoverThumb::draw` | ricostruire `build_launcher_icons.py` / `build_block_artwork.py`; unificare bit-order |
| Reader in landscape | alta | `Gfx.{h,cpp}`, `Scene.{h,cpp}`, `ReaderScene.cpp`, `Input.h`, `Sleep.cpp`, `PrioritiesScene.cpp` | rotazione parametrica + allineamento partial 8 px; decisione su barra soft-key e mapping tasti; re-index cache (automatico via header viewport) |
| Modalità dashboard (USB) | media | `main.cpp:588-609`, `:644-653`, `Scene.cpp:136-216`, nuova scena | alimentazione USB (il latch GPIO13 esclude il wake da timer su batteria); politica di scrub/anti-ghosting |
