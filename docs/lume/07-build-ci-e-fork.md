# Build, toolchain, CI/release e guida al fork

Riferimenti: `xphone-os/platformio.ini`, `xphone-os/tools/`, `.github/workflows/firmware-release.yml`, `LICENSE`, `freeink-sdk/LICENSE`+`NOTICE`. Per l'architettura runtime vedi [architettura firmware](02-architettura-firmware.md), per il protocollo companion [protocollo BLE](03-protocollo-ble.md), per pannelli/OTA [hardware, power, OTA](06-hardware-power-ota.md).

## 1. Toolchain

| Voce | Valore | File |
|---|---|---|
| Piattaforma | `platform-espressif32.zip` release **55.03.37** di `pioarduino` (URL diretto, non registry) | `platformio.ini:15` |
| Framework | `arduino` (core ESP32 pioarduino; BLE compilato su NimBLE su C3) | `platformio.ini:17`, `:76-82` |
| Board | `esp32-c3-devkitm-1` | `platformio.ini:16` |
| Monitor / upload | 115200 baud / 921600 baud | `platformio.ini:18-19` |
| Flash | 16 MB, mode `dio`, offset app `0x10000` | `platformio.ini:21-27` |
| Partizioni | `partitions.csv`: nvs `0x5000`, otadata `0x2000`, app0/app1 `0x640000` ciascuna, spiffs `0x360000`, coredump `0x10000` | `partitions.csv:2-7` |

Il pin della piattaforma via URL è deliberato: la patch al framework (§1.3) dipende da un file esatto di quel pacchetto.

### 1.1 Build flags, uno per uno (`platformio.ini:38-63`)

| Flag | Cosa fa | Perché |
|---|---|---|
| `-DARDUINO_USB_MODE=1` | usa il controller USB-Serial/JTAG nativo del C3 invece di un ponte esterno | flash e log passano dal solo connettore disponibile (`:39`) |
| `-DARDUINO_USB_CDC_ON_BOOT=1` | `Serial` è la CDC USB già al boot | il log di boot è visibile senza UART fisica (`:40`) |
| `-DEINK_DISPLAY_SINGLE_BUFFER_MODE=1` | un solo framebuffer (~52 KB X3 / 48 KB X4); il frame precedente resta nella RAM del pannello | su 320 KB senza PSRAM il doppio buffer non entra; il flag è consumato dall'SDK in `freeink-sdk/libs/display/FreeInkDisplay/include/FreeInkDisplay.h:70,93,151` (`:41-42`) |
| `-DXML_GE=0` | disabilita le general entities di expat | superficie d'attacco e dimensione del parser sugli EPUB non fidati (`:43-49`) |
| `-DXML_CONTEXT_BYTES=1024` | finestra di contesto d'errore expat = 1 KB | valore identico al progetto sorgente `x4-os`, così il parser si compila uguale (`:50`) |
| `-DPNG_MAX_BUFFERED_PIXELS=16416` | spazio per due scanline RGBA filtrate fino a 2048 px nella struct del decoder PNGdec | il default `(320*4+1)*2` rifiuta le larghezze tipiche delle copertine (`:51-54`) |
| `-DUSE_UTF8_LONG_NAMES=1` | SdFat accetta nomi lunghi UTF-8 | senza, `open(O_CREAT)` rifiuta qualsiasi nome non-ASCII e i libri con accenti risultano "unreadable" (`:55-60`) |
| `-std=gnu++2a` | C++20 con estensioni GNU | il codice usa costrutti oltre gnu++11 (`:61`) |
| `-fno-exceptions` | nessuna eccezione C++ | flash/RAM: nessuna unwind table; il codice usa codici di ritorno (`:62`) |
| `-flto` | link-time optimization | "flash diet" M2.1c: elimina codice cross-TU non usato (`:63`) |

`build_unflags = -std=gnu++11 -fexceptions` (`:65-67`) rimuove i default che il core Arduino inietta: senza l'unflag il compilatore riceverebbe due `-std` (vince l'ultimo, fragile) e `-fexceptions` annullerebbe `-fno-exceptions`.

Nessun `CONFIG_BT_*` è necessario: arrivano dallo `sdkconfig.h` precompilato del core (`platformio.ini:78-82`).

### 1.2 LTO e lo script di link

`-flto` in `build_flags` finisce solo in `CCFLAGS`: SCons `ParseFlags` non mappa i `-f*` in `LINKFLAGS`, quindi `ld` vedrebbe oggetti LTO "magri" senza plugin e fallirebbe con `plugin needed to handle lto object` / `undefined reference to app_main`. `tools/lto_link.py:10-11` fa `env.Append(LINKFLAGS=["-flto"])` come script `post:` (`platformio.ini:36`), così il driver gcc carica `liblto_plugin` al link.

### 1.3 Patch al framework BLE

`tools/patch_ble_service_friend.py` è uno script `pre:` (`platformio.ini:35`). Inserisce **una riga** `friend class CompanionBleService;` dopo l'ancora `friend class BLEDevice;` in `<pkg framework-arduinoespressif32>/libraries/BLE/src/BLEService.h` (`patch_ble_service_friend.py:21-36`).

- Motivo: `BLEServer` upstream non ha distruttore, `BLEDevice::deinit` orfanizza `BLEService`/`BLECharacteristic` → 1,76 KB persi per ciclo init/deinit, e il reader sospende/riprende la radio a ogni sessione di lettura (`patch_ble_service_friend.py:3-9`).
- Idempotente (rileva il marker) e **fail-loud**: se l'ancora non c'è, `raise RuntimeError` e la build muore invece di degradare silenziosamente (`:31-35`).
- Rischi in un fork: (a) la patch scrive nel **pacchetto condiviso** `~/.platformio/packages/...`, quindi contamina ogni altro progetto sulla stessa macchina che usa quel framework; (b) qualsiasi bump di piattaforma richiede di ri-verificare il leak e l'ancora; (c) la CI mette `~/.platformio` in cache (`firmware-release.yml:33-36`), quindi la patch viaggia nella cache.

## 2. Env PlatformIO Lume

| Env | Define aggiunti | Immagine prodotta |
|---|---|---|
| `lume-x3-it` (default) | `FREEINK_DEVICE_X3=1`, `FREEINK_BATTERY_I2C_GAUGE=1`, `LUME_LOCALE_IT=1` | X3, solo italiano |
| `lume-x3-en` | `FREEINK_DEVICE_X3=1`, `FREEINK_BATTERY_I2C_GAUGE=1`, `LUME_LOCALE_EN=1` | X3, solo inglese |

I due env differiscono soltanto per la lingua, scelta nel preprocessore da
`src/LumeLocale.h`. Il binario non contiene i letterali della lingua esclusa.
Non esistono più env X4 o immagine universale nel fork Lume. Il guard hardware
continua a verificare il fingerprint X3 prima di inizializzare UC8253.

### 2.1 `lib_deps` (`platformio.ini:83-90`)

Sei librerie dell'SDK vendorizzato entrano via **symlink**: `BoardConfig`, `EInkDisplay` (`FreeInkDisplay`), `SDCardManager`, `InputManager`, `BatteryMonitor`, `XteinkDetect`, tutte `symlink://../freeink-sdk/libs/...`. `SDCardManager/library.json` tira `greiman/SdFat ^2.3.1` dal registry. Unica dipendenza esterna dichiarata: `bblanchon/ArduinoJson @ 7.4.2` (protocollo card/command). Le librerie sotto `xphone-os/lib/` sono compilate automaticamente da PlatformIO (LDF) senza comparire in `lib_deps`.

## 3. Comandi pratici

```sh
cd xphone-os
../.venv/bin/pio run -e lume-x3-it -e lume-x3-en
../.venv/bin/pio run -e lume-x3-it -t upload --upload-port /dev/cu.usbmodem*
../.venv/bin/pio device monitor --baud 115200
```

Requisiti macOS: Python 3.11, PlatformIO 6.1.19 nella `.venv`, nessun driver
seriale extra. Il flash USB richiede il cavo pogo a 4 pin; il deep sleep chiude
la porta, quindi premere power prima dell'upload.

Da Settings → SD Firmware Update si può scegliere direttamente
`update_it.bin` o `update_en.bin`. Il boot updater Left + Power cerca invece
`/update.bin`: rinominare il file scelto, oppure usare lo zip locale prodotto
dalla release. Un erase completo non è un normale aggiornamento: cancella
anche bond BLE, slot OTA e spiffs.

## 4. CI / release

`.github/workflows/firmware-release.yml` (`Lume Firmware Release`) parte su tag
`fw-v*` o `workflow_dispatch`, usa Python 3.11 e PlatformIO 6.1.19, quindi:

1. compila `lume-x3-it` e `lume-x3-en`;
2. pubblica `update_it.bin`, `update_en.bin`, `lume-x3-it.bin`,
   `lume-x3-en.bin` e copie versionate;
3. crea `lume-x3-it.zip` e `lume-x3-en.zip`, ciascuno con il corretto
   `update.bin` e le istruzioni del boot updater;
4. calcola `SHA256SUMS.txt`;
5. genera `latest.json` con `assets.x3.defaultLocale = "it"` e
   `assets.x3.locales.it/en`;
6. crea la GitHub Release tramite `softprops/action-gh-release@v2`.

Per un fork: abilitare Actions, concedere al `GITHUB_TOKEN` permessi contents
read/write e taggare `fw-vX.Y.Z`. Nessun segreto applicativo è richiesto. Il
workflow non verifica la corrispondenza tra tag e `XPHONE_VERSION`: allinearli
prima del tag.

## 5. Licenze

| Componente | Licenza | Evidenza | Obblighi in redistribuzione |
|---|---|---|---|
| Flowe OS (`xphone-os/`) | MIT — © 2026 Andrew Jiang | `LICENSE` | mantenere copyright + testo MIT |
| FreeInk SDK (`freeink-sdk/`) | MIT — © 2026 FreeInk | `freeink-sdk/LICENSE` | mantenere `LICENSE` **e** `NOTICE` |
| Sequenze init/LUT pannelli SSD1677 e UC8253 | MIT — OpenX4 E-Paper Community SDK, attribuzione a CidVonHighwind | `freeink-sdk/NOTICE:9-22` | conservare l'attribuzione di terza parte nel NOTICE |
| JPEGDEC 1.2.7 (vendorizzato, patch progressive-MCU-skip già applicate) | Apache-2.0 — © 2020 BitBank Software | `xphone-os/lib/JPEGDEC/library.json`, `.../LICENSE` | conservare LICENSE, dichiarare le modifiche |
| PNGdec 1.1.6 (vendorizzato, trimmato) | Apache-2.0 — © 2020 BitBank Software | `xphone-os/lib/PNGdec/library.json`, `.../LICENSE` | come sopra |
| expat 2.7.3 (vendorizzato, `xmlparse.c` alterato) | MIT — notice per-file | `lib/expat/expat.h:23-42` | conservare gli header di licenza dei file |
| uzlib 2.9.8 | Zlib | `lib/uzlib/src/tinflate.c:11-30` | non spacciarsi per autori; **marcare le versioni alterate** |
| ArduinoJson 7.4.2, SdFat 2.3.1 | dipendenze dal registry, non vendorizzate | `platformio.ini:86,90` | licenze upstream (MIT) valgono per il binario distribuito |
| Motore EPUB | derivato da CrossPoint (MIT) | `README.md:119-121` | citare CrossPoint |

Un fork che ridistribuisce **binari** deve comunque veicolare le licenze MIT/Apache/Zlib dei componenti compilati; un fork che ridistribuisce **sorgente** deve tenere `LICENSE`, `freeink-sdk/LICENSE`, `freeink-sdk/NOTICE`, i `LICENSE` di JPEGDEC/PNGdec e gli header di expat/uzlib. Il rebranding può cambiare nome del progetto ma non rimuovere il copyright originale.

## 6. Guida al fork

### 6.1 Rebranding — punti esatti

| Cosa | File:riga | Nota |
|---|---|---|
| Nome BLE annunciato | `src/ble/CompanionProtocol.h:25` — `"xphone X3"` / `"xphone X4"` | usato in `BLEDevice::init` (`src/ble/CompanionBleService.cpp:187`), scan response (`:262`), stringa di stato "Advertising as …" (`:342`) |
| UUID servizio/caratteristiche | `src/ble/CompanionProtocol.h:22-24` | Lume usa UUID propri, replicati in `ios/Lume/Bluetooth/LumeProtocol.swift`, per evitare che la vecchia app Flowe risponda agli stessi comandi e sovrascriva gli snapshot |
| Logo boot (bitmap 120×120, 1bpp) | `src/art/FloweLogo.h:9-12` | blitter `src/main.cpp:76-84`, posizionamento `:90` |
| Wordmark splash | `src/main.cpp:91` — `drawTextCentered(kFontBold, cx, wordmarkY, "flowe")` | |
| Wordmark status bar launcher | `src/scenes/LauncherScene.cpp:173` — `drawText(kFontBold, …, "flowe")` | sole + wordmark, commento `:158-161` |
| Versione firmware | `src/scenes/AppScenes.h:10` — `XPHONE_VERSION = "0.5.0"` | mostrata in `SettingsScene.cpp:288,301`, `AboutScene.cpp:35`, ed esposta via HTTP in `net/FileTransferServer.cpp:70,146,158` |
| Stringhe UI "Flowe app" | `src/scenes/FileTransferScene.cpp:271,282,286,309,323`; `src/net/FileTransferServer.cpp:146` | testo mostrato all'utente sul device |
| Etichetta prodotto "xphone-os" | `src/scenes/SettingsScene.cpp:301`; `src/net/FileTransferServer.cpp:146` | |
| Nomi file di release | `.github/workflows/firmware-release.yml:62-83` (`flowe-x3.bin`, `flowe-x4.bin`, `update.bin`, `flowe-x3.zip`), testo README dello zip `:74-82`, manifest `:109-127` | `update.bin` è il nome che l'updater SD cerca (`src/SdUpdate.cpp:33`): rinominarlo rompe il flash da SD |
| Link/contatti issue | `.github/ISSUE_TEMPLATE/config.yml` (URL `flowe.ink`, repo `andrewjiang/flowe-os`) | |

### 6.2 Aggiungere una scena/app

1. `src/scenes/MiaScene.{h,cpp}`: sottoclasse di `Scene` — obbligatori `handleInput(Input&)` e `render(Gfx&)` (`src/Scene.h:46-49`); opzionali `onEnter/onExit` (`:23-24`), `softKeys()` (`:31`), `longPressSlots()` (`:37`), `softKeyIconMask()` (`:41`). Il contenuto deve stare sopra `gfx.height() - Scene::SOFTKEY_BAR_H` (44 px, `Scene.h:21`).
2. Registrare l'id: nuovo valore in `enum class SceneId` (`src/scenes/AppScenes.h:16-27`; i valori sono espliciti perché finiscono in RTC memory al sleep — **non riordinarli**), dichiarare `void showMiaScene();` accanto agli altri (`:42-56`), aggiungere il nome in `sceneName()`.
3. `src/scenes/AppScenes.cpp`: istanza statica nel namespace anonimo (`:24-34`, nessun heap), `showMiaScene()` che imposta `gCurrentSceneId` e chiama `SCENES.switchTo(...)` (pattern di `:37-40`), e un `case` in `showSceneById()` (`:107-122`) per il restore da deep sleep.
4. Launcher: aumentare `LauncherScene::APP_COUNT` (`src/scenes/LauncherScene.h:17`, oggi 6 con `COLS = 2`), aggiungere il nome in `kApps[]` (`src/scenes/LauncherScene.cpp:22-24`) e il ramo `strcmp` in `handleInput` (`:126-140`).
5. Icona: `XPhoneIconAppCount` (`src/art/LauncherIcons.h:10`, oggi 6) va allineato e serve un bitmap 104×104 1bpp **per ognuno dei 5 pack** (`:11-20`, tabella `XPhoneIconPacks` a `:2274`), altrimenti `IconStyle::iconForApp()` (`src/IconStyle.cpp:57`) indicizza fuori dalla riga del pack.
6. Soft-key: 4 etichette statiche nell'ordine Back/Confirm/Left/Right (`Scene.h:26-31`); lo slot 0 long-press è sempre "vai al launcher" e non è sovrascrivibile (`Scene.h:33-37`).
7. Se la scena è alimentata da una card BLE, aggiungere un `case` in `CompanionSync::requestForSceneId()` (unico punto di registrazione del resync al wake, `src/CompanionSync.h:23-25`).

### 6.3 Aggiungere un tipo di card BLE

Schema e semantica: [protocollo BLE](03-protocollo-ble.md). Lato firmware i punti da toccare sono:

1. `src/ble/CompanionProtocol.h` — struct dei nuovi item e i relativi cap (`MAX_*`, es. `MAX_CARD_BYTES = 4096`, `MAX_WORKOUT_ITEMS = 8`, `:30-44`); i cap sono i limiti reali del parser, non suggerimenti.
2. `CompanionBleService::applyCardPayload()` (`src/ble/CompanionBleService.cpp:759-1029`) — parsing degli array JSON con `clippedString(...)` per ogni campo (modello: workout `:956-976`) e un blocco di *service-level capture* verso uno store dedicato, con predicato su `kind`/prefisso di `id` (modelli: priorities `:984-986`, today `:994-997`, workout `:1000-1003`, block `:1010-1012`). La cattura fuori dallo slot singolo della card è obbligatoria: il push successivo lo sovrascrive.
3. Store persistente `src/XxxStore.{h,cpp}` con `updateFromCard(const CompanionCardState&)` (modelli: `TodayStore`, `WorkoutStore`, `BlockStatusStore`).
4. Redraw: un `markXxxDirtyIfActive()` in `src/scenes/AppScenes.h:62-71`, chiamato dal pump di revisione in `main.cpp`.
5. Comandi in uscita: `sendCommand("xxx.sync.request")` sul modello di `:1078-1116`; il tipo del comando è la stringa `doc["type"]` (`:1138`).
6. Dispatch dei comandi **in ingresso** dal telefono: catena `std::strcmp(type, …)` a `:784-836` (`reader.shelf.request`, `notif.filter`, `notif.apps.request`, `time.sync`, `transfer.start/stop/wifi`).

### 6.4 Restare sincronizzati con upstream

La repo pubblica non è la repo di sviluppo: è un export. 13 commit totali, di cui i rilasci sono **squash singoli** dal monorepo privato, es. `3101448 Sync from flowe monorepo @ 465d9f3: universal image + reliability (fw-v0.5.0)` (32 file, ~600 righe) e `efd94a0 … (fw-v0.4.0)` (20 file, 769 inserzioni). Il messaggio cita l'hash del monorepo (inaccessibile) e il changelog sta nel corpo del commit / nelle release notes. Non esistono branch upstream oltre `main`.

Strategia concreta:

```sh
git remote add upstream https://github.com/andrewjiang/flowe-os.git
git fetch upstream --tags
git switch -c fork/main upstream/main        # base pulita = un commit di sync
# lavoro del fork in branch tematici, uno per area
git switch -c patch/rebrand fork/main
```

- **Merge, non rebase**, di `upstream/main` nel proprio `main`: ogni sync è un commit-bomba unico e un rebase dei propri commit sopra di esso rigioca ogni patch contro un diff enorme, moltiplicando i conflitti. Con `merge` si risolve una volta.
- Tenere le modifiche del fork in **branch di patch tematici** e piccoli (rebranding, nuove scene, fix), rebasati solo tra loro: dopo ogni sync si ri-mergiano uno alla volta e si vede subito quale area collide.
- Leggere cosa è cambiato realmente fra due rilasci: `git diff efd94a0 3101448 --stat` per l'ambito, `git diff efd94a0 3101448 -- xphone-os/src/ble/` per file mirati, `git show 3101448 --stat` per il changelog nel corpo del commit. Non esiste una storia più fine di così: la bisezione fine-grana su upstream è impossibile, quindi vale la pena isolare i propri commit per poterli bisezionare *nel fork*.
- `git tag` upstream usa lo schema `fw-vX.Y.Z`: usare un prefisso diverso nel fork (es. `myfork-vX.Y.Z`) evita che `git fetch --tags` collida e che il proprio workflow di release parta su un tag upstream.
- Prima di ogni merge, controllare `platformio.ini` e `tools/patch_ble_service_friend.py`: un bump di piattaforma upstream è la modifica che più facilmente rompe una build locale (§7).

## 7. Rischi e attriti noti (con mitigazione)

| Rischio | Evidenza | Mitigazione |
|---|---|---|
| Patch al framework BLE si rompe su upgrade di piattaforma | `tools/patch_ble_service_friend.py:31-35` | lo script fallisce forte: al bump, ri-misurare il leak e riportare la patch; alternativa strutturale è non chiamare `deinit` ma tenere la radio viva |
| Patch che sporca il pacchetto condiviso | scrive in `PioPlatform().get_package_dir(...)` (`:25-27`) | usare un `core_dir` per progetto, oppure `pio pkg` in un venv dedicato |
| `lib_deps` a symlink relativi (`../freeink-sdk/...`) | `platformio.ini:84-89` | l'albero deve restare intatto: copiare solo `xphone-os/` rompe la build; in un fork che sposta l'SDK, aggiornare tutti e sei i path |
| RAM: 320 KB, no PSRAM | `-DEINK_DISPLAY_SINGLE_BUFFER_MODE=1` (`:42`); ~96,5 KB usati (`xphone-os/README.md:55-56`) | ogni feature nuova va misurata; niente buffer statici grandi, cap espliciti come i `MAX_*` del protocollo |
| Cavo pogo 2 pin = solo carica | `README.md:114-116` | procurare il 4 pin, oppure usare sempre il path SD |
| SD/lettore inaffidabile | `xphone-os/README.md:68-70` ("SD/reader is flaky — verify (unmount/remount + cmp)") | dopo la copia di `update.bin`, rimontare e `cmp` col file locale prima di tentare il flash |

## Cose da sistemare / attriti

1. **`x3` e `x4` producono lo stesso binario, ma la release lo pubblica come due file diversi.** Tutti gli env definiscono `FREEINK_DEVICE_X3=1` e `FREEINK_DEVICE_X4=1` (`platformio.ini:99-100,114-115,122-123`) e `FREEINK_BATTERY_I2C_GAUGE` è già implicato da `FREEINK_DEVICE_X3` (`freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h:158-159`); la CI compila due volte e produce `flowe-x3.bin`/`flowe-x4.bin` (`firmware-release.yml:57,62-63`) suggerendo all'utente una scelta che non esiste. Gravità: media. Fix: un solo env (`xteink`) con un solo artefatto `flowe.bin` + `update.bin`, alias env rimossi.
2. **Cache CI mai aggiornata → rischio di binario stale nella release.** La chiave è `hashFiles('xphone-os/platformio.ini')` (`firmware-release.yml:36`) ma il path in cache include `xphone-os/.pio/build`: finché `platformio.ini` non cambia la chiave fa hit e la cache non viene mai riscritta, quindi ogni release riparte da una `.pio/build` di un commit arbitrario. Gravità: alta (con LTO gli artefatti intermedi sono grandi e la fiducia sta tutta in SCons). Fix: rimuovere `xphone-os/.pio/build` dalla cache e cachare solo `~/.platformio`.
3. **Nessuna verifica che il tag corrisponda alla versione compilata.** `RELEASE_TAG`/`VERSION` vengono dal tag (`firmware-release.yml:45-51`) mentre la versione nel firmware è la costante `XPHONE_VERSION = "0.5.0"` (`src/scenes/AppScenes.h:10`); un tag `fw-v0.6.0` produce un device che dice "0.5.0" in About e su `GET /health`. Gravità: alta (supporto: la versione riportata dall'utente è falsa). Fix: step che confronta il define col tag e fallisce, o `-DXPHONE_VERSION` iniettato dal tag.
4. **Nessuna CI su push/PR.** L'unico workflow parte su tag `fw-v*` o dispatch (`firmware-release.yml:3-12`): un fork può mergiare codice che non compila e scoprirlo solo al momento del rilascio. Gravità: media. Fix: workflow `on: [push, pull_request]` che esegue `pio run -e xteink`.
5. **Lo zip esiste solo per X3, ma il README dice di scaricare "lo zip per il tuo device".** `README.md:87` promette `flowe-x3` o `flowe-x4`; la CI crea solo `flowe-x3.zip` (`firmware-release.yml:71-83`). Anche `latest.json` non elenca né lo zip né `update.bin` (`:109-127`). Gravità: media. Fix: unico zip (vista la nota 1) e README allineato.
6. **README stale sul nome BLE annunciato.** `xphone-os/README.md:239-240` dice che il device si annuncia come `X4 Companion`; il codice annuncia `"xphone X3"`/`"xphone X4"` (`src/ble/CompanionProtocol.h:25`). Chi riscrive l'app companion, cercando per nome, non trova nulla. Gravità: media. Fix: aggiornare il README e ribadire che la scoperta va fatta per SERVICE_UUID.
7. **Comandi di build contraddittori nel README radice.** `README.md:107-109` elenca `pio run -e x4` due volte, una volta come "hardware-validated" e una come "compiles; not validated on hardware"; `xphone-os/README.md:2-5` afferma solo-X3, mentre il commit `e8b43a5` dichiara "X3 and X4 both hardware-validated". Gravità: bassa. Fix: una sola riga per env e uno stato hardware unico.
8. **Tool di generazione degli asset assenti dalla repo pubblica.** `src/art/LauncherIcons.h:4` rimanda a `tools/xphone-icons/build_launcher_icons.py`, `src/art/BlockArtwork.h:7-8` a `tools/x4-block-assets/` e `tools/x4-screen-lab/`: in `xphone-os/tools/` ci sono solo `lto_link.py`, `patch_ble_service_friend.py`, `subset_epd_font.py`. Un fork non può rigenerare icone né artwork. Gravità: alta per chi vuole ridisegnare la UI. Fix: esportare quegli script nel sync, o documentare il formato (104×104, 1bpp MSB-first, bit 0 = ink) come contratto d'ingresso.
9. **`subset_epd_font.py` richiede input che non esistono nella repo pubblica.** Le invocazioni documentate puntano a `../x4-os/lib/EpdFont/builtinFonts/*.h` (`tools/subset_epd_font.py:26-32`), path del monorepo privato. Gravità: media (blocca l'aggiunta di glifi, es. cirillico/greco). Fix: vendorizzare i font originali o documentare come rigenerarli con `fontconvert.py`.
10. **`board_upload.maximum_size = 16777216` scavalca il limite reale della partizione app.** Le partizioni app sono `0x640000` = 6 553 600 B (`partitions.csv:4-5`, coerente con le percentuali di `xphone-os/README.md:261-264`), mentre la voce dichiara 16 MB (`platformio.ini:22`): è config morta nel migliore dei casi, un guard-rail disattivato nel peggiore. Gravità: bassa. Fix: rimuovere la riga e lasciare che il limite derivi da `partitions.csv`.
11. **`.gitignore` ignora `*.bin` in tutto l'albero.** `.gitignore:2`: qualsiasi asset binario che un fork volesse versionare (fixture `section.bin` del reader, immagini di test) va aggiunto con `git add -f` o scompare silenziosamente. Gravità: bassa. Fix: restringere a `xphone-os/.pio/**/*.bin` e `dist/*.bin`.
12. **Sorgente vendorizzato alterato senza traccia della patch.** `lib/expat/xmlparse.c:145-160` ha il blocco `#error` sull'entropia commentato via (indebolisce il salt anti-hash-DoS: `gather_time_entropy() ^ getpid()`, `:1075,1137`) e la patch è documentata solo in un commento di `platformio.ini:44-47`; JPEGDEC dichiara nel proprio `library.json` di avere già dentro le patch CrossPoint. Un upgrade della libreria perde le modifiche o le reintroduce a caso. Gravità: media. Fix: tenere le patch come file `.patch` in `tools/` e riapplicarle in modo verificabile.
13. **La patch al framework scrive in una directory globale condivisa.** `tools/patch_ble_service_friend.py:24-27` modifica il pacchetto `framework-arduinoespressif32` nella cartella PlatformIO dell'utente: effetto collaterale su ogni altro progetto della stessa macchina, e patch invisibile nella build directory. Gravità: media. Fix: `core_dir` per progetto in `[platformio]`, o copia locale della libreria BLE.
