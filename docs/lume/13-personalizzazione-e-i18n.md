# Personalizzazione e localizzazione (fork X3-only)

Analisi di fattibilità su `flowe-os/xphone-os/` (repo read-only). Ogni affermazione è ancorata a `file:riga`. Marcatura: fatti verificati senza prefisso, deduzioni con `[INFERENZA]`. Fuori scope: protocollo BLE e OTA.

Contesto di piattaforma rilevante per tutti i punti:
- Coordinate logiche **portrait** 528x792 su X3; `Gfx::begin()` scambia larghezza/altezza del pannello nativo (`src/Gfx.cpp:6-14`) e `Gfx::drawPixel()` ruota 90° CW verso il framebuffer nativo landscape (`src/Gfx.cpp:16-32`).
- Render **solo su dirty flag**, nessun redraw periodico (`src/Scene.h:5-11`, `src/main.cpp:653`); scene statiche, nessuna heap (`src/Scene.h:11`).
- Partizione app 6.5 MB (`xphone-os/partitions.csv`: `app0 0x640000`); l'immagine attuale usa ~2.5 MB e ~146 KB di RAM (`README.md:70-72`) → ~4 MB di flash liberi.

---

## 1. Localizzazione italiana a compile time — implementata

### Decisione

Il selettore runtime descritto nella prima analisi è stato scartato. Lume produce
un'immagine per lingua:

| Env PlatformIO | Definizione | Artefatto diretto |
|---|---|---|
| `lume-x3-it` (default) | `LUME_LOCALE_IT=1` | `update_it.bin` |
| `lume-x3-en` | `LUME_LOCALE_EN=1` | `update_en.bin` |

`src/LumeLocale.h` espone `L10N(english, italian)`. La scelta avviene nel
preprocessore, prima della compilazione: il linker non riceve il letterale della
lingua esclusa. Non esistono lingua in NVS, BSS aggiuntiva, cambio a caldo o
seconda tabella di puntatori. Definire entrambe le macro genera `#error`; una
build ad hoc senza macro resta italiana.

La scelta è stata verificata sui binari, non solo sul sorgente:

* `update_it.bin` contiene `Impostazioni`, `Nessuna notifica`,
  `Priorità di oggi`, `Allenamento di oggi`, `Nessun libro trovato` e non le
  corrispondenti stringhe inglesi;
* `update_en.bin` mostra il risultato opposto;
* entrambe le immagini superano build PlatformIO e validazione
  `esptool image-info` come ESP32-C3, checksum e validation hash validi.

### Superfici localizzate

La localizzazione copre splash e riavvio, sleep face, barra soft-key, launcher e
tutte le scene: Notifications, Priorities, Today, Workout, Block/Focus, Reader,
File Transfer, Settings e About. Sono localizzati titoli, stati vuoti,
richieste/sincronizzazione, errori, conferme, contatori, statistiche reader,
giorni della settimana e nomi scena diagnostici.

Le label soft-key rispettano la larghezza del tab X3: `INDIETRO`, `APRI`,
`PREC`, `SUCC`, `SU`, `GIÙ`, `SINC`, `FATTO`, `SCEGLI`, `AVVIA`, `MODALITÀ`,
`PAUSA`, `LIBRI`, `TESTO`, `ELIMINA`, `ANNULLA`, `ESCI`, `RIPROVA`, `IMPOSTA`,
`SÌ`, `NO`, `+SERIE`. I font UI includono Latin-1 e quindi renderizzano
correttamente `à`, `è`, `ì`, `ù` e le maiuscole accentate.

### Invarianti preservate

Non sono stati tradotti i token macchina del protocollo e delle euristiche:
`"reminder"`, `"All day"`, `"active"`, `"break"`, `"ready"`, suffissi
`" sent"`/`" received"`, tipi card, UUID, chiavi JSON/NVS e stati del file
transfer. Questo evita di rompere:

* il filtro reminder/all-day;
* il riconoscimento delle card Block;
* la direzione delle frecce di trasferimento;
* il contratto BLE con l'app.

Le intestazioni canoniche Today ricevute come `TODAY`, `TONIGHT`, `TOMORROW`
sono confrontate nella forma di protocollo e tradotte soltanto al render in
`OGGI`, `STASERA`, `DOMANI`.

I formatter variadici conservano anche gli argomenti non mostrati nella forma
italiana: `%.0s` consuma il suffisso plurale inglese senza stamparlo. Rimuovere
semplicemente `%s` avrebbe fatto leggere a `%lu` un puntatore, causando
comportamento indefinito.

### Dati ancora prodotti dall'iPhone

`TodayStore::Item::time`, `TodayStore::syncLine` e
`BlockStatusStore::endsAtLabel` arrivano già formattati dal companion. La
localizzazione firmware non può trasformarli senza perdere semantica; l'app
Lume italiana li produce nel formato locale/24 ore. I valori canonici usati
per filtri e routing restano invece indipendenti dalla lingua.

### Build e release

Build locale:

```sh
cd xphone-os
../.venv/bin/pio run -e lume-x3-it
../.venv/bin/pio run -e lume-x3-en
```

Il workflow `.github/workflows/firmware-release.yml` compila entrambi gli env e
pubblica:

* `update_it.bin`, `update_en.bin`;
* `lume-x3-it.bin`, `lume-x3-en.bin` e copie versionate;
* `lume-x3-it.zip`, `lume-x3-en.zip`, ciascuno con il proprio `update.bin`;
* `SHA256SUMS.txt`;
* `latest.json` con `assets.x3.defaultLocale = "it"` e le due entry
  `assets.x3.locales.it/en`.

Le immagini dirette con suffisso lingua si scelgono da
Settings → SD Firmware Update. Il boot updater storico cerca invece il nome
esatto `update.bin`: per quel percorso usare lo zip della lingua scelta.

### Verifica hardware residua

La build e l'isolamento delle lingue sono verificati. Il flash e la revisione
visiva italiana sul vero X3 restano da eseguire quando il device è collegato:
controllare soprattutto larghezza delle soft-key, righe About più lunghe,
messaggi vuoti e glifi accentati. Non dichiarare questa parte completata sulla
sola base della build.
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
| i18n IT/EN a compile time | **implementata e verificata su X3** | `src/LumeLocale.h`, `src/scenes/*.cpp`, `Scene.cpp`, `Sleep.cpp`, `main.cpp`, `platformio.ini`, workflow release | build `lume-x3-it/en`; smoke test italiano sul vetro superato; token protocollo lasciati canonici; app responsabile dei campi già formattati |
| Font UI con nuovi range (€, altri) | media | nuovo `fontconvert` non compresso; `tools/subset_epd_font.py:42`; `src/fonts/*`, `Fonts.cpp` | tool di generazione (assente in repo) |
| Font reader aggiuntivi (flash) | media | nuovo `fontconvert` 2bpp+DEFLATE; `reader/ReaderFonts.{h,cpp}`, `ReaderScene.cpp:645-656`, `:76` | ~50-80 KB flash per stile-misura; consapevolezza che `fontId` è chiave cache `section.bin` |
| Font reader da SD a runtime | alta | `FontDecompressor.{h,cpp}`, `EpdFont.cpp`, nuovo `SdCardFont` | sorgente inflate streaming da file; budget RAM (max gruppo 38 KB + tabella glifi 17 KB) |
| Layout sleep multipli / poster blocco | bassa | `Sleep.cpp:94-133`, nuovo `SleepFaces`, `SettingsScene.cpp` | chiave NVS `sleepFace`; nessun orologio (no RTC) |
| Icona sync/caricamento personalizzata | bassa | `SyncIndicator.h:87-116`, `StatusBar.h:92-111` | nessuna animazione senza refresh periodico |
| Icone launcher / artwork da SD | bassa/media | `IconStyle.{h,cpp}`, `LauncherScene.cpp:34-45`, riuso `CoverThumb::draw` | ricostruire `build_launcher_icons.py` / `build_block_artwork.py`; unificare bit-order |
| Reader in landscape | alta | `Gfx.{h,cpp}`, `Scene.{h,cpp}`, `ReaderScene.cpp`, `Input.h`, `Sleep.cpp`, `PrioritiesScene.cpp` | rotazione parametrica + allineamento partial 8 px; decisione su barra soft-key e mapping tasti; re-index cache (automatico via header viewport) |
| Modalità dashboard (USB) | media | `main.cpp:588-609`, `:644-653`, `Scene.cpp:136-216`, nuova scena | alimentazione USB (il latch GPIO13 esclude il wake da timer su batteria); politica di scrub/anti-ghosting |
