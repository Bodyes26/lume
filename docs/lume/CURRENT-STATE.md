# Lume — stato operativo e handoff

**Aggiornato:** 13 agosto 2026  
**Versione firmware:** `0.1.0-dev`  
**Versione app iOS:** `0.2.0-dev`  
**Base upstream:** `andrewjiang/flowe-os@3101448b02362e627cb17c4de863c1ed22d2478d` (`fw-v0.5.0`)  
**Fase:** firmware v0.1 accettato; primo vertical slice dell'app iOS v0.2 implementato e verificato in Simulator.

Questo file descrive soltanto lavoro realmente osservato. Per riprendere da una nuova
sessione, partire da [START-HERE.md](START-HERE.md).

## Stato semaforo

| Area | Stato | Evidenza / limite |
|---|---|---|
| Documentazione persistente | **VERIFICATA** | 14 capitoli + START/HANDOFF dentro `docs/lume/` |
| Toolchain locale | **VERIFICATA** | Python 3.11, `.venv`, PlatformIO Core 6.1.19 |
| Build upstream di riferimento | **VERIFICATA** | `xteink` prima delle modifiche: SUCCESS; flash 2.532.155 B, RAM 145.004 B |
| Build Lume X3-only | **VERIFICATA** | ultimo build: SUCCESS; flash 2.526.395 B, RAM 144.988 B |
| Identità a compile time | **VERIFICATA** | stringhe, UUID preservati, asset/env/release compilano |
| Boot e resa sul vetro | **VERIFICATA DALL'UTENTE** | Maurizio ha provato le schermate e confermato il funzionamento complessivo |
| BLE/ANCS reale v0.1 | **VERIFICATA** | nome advertising `Lume X3`; notifica WhatsApp ricevuta e renderizzata |
| App iOS Lume v0.2 | **PARZIALMENTE VERIFICATA** | build iPhone unsigned SUCCESS; 4 contract test PASS; UI Simulator ispezionata. Installazione reale richiede account/profilo firma e Xcode con supporto iOS 27 |

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
* Versione a schermo/protocollo HTTP: `0.1.0-dev` in
  `xphone-os/src/scenes/AppScenes.h` (il simbolo storico `XPHONE_VERSION` è mantenuto
  per ridurre il diff upstream; non è testo visibile).
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

* Un solo env PlatformIO: `lume-x3`, default, con `FREEINK_DEVICE_X3=1` e senza
  `FREEINK_DEVICE_X4`. Il compilatore elimina SSD1677 e i rami/artwork X4.
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

* `.github/workflows/firmware-release.yml` compila solo `lume-x3` e produce:
  `lume-x3.bin`, `lume-x3-<version>.bin`, `update.bin`, `lume-x3.zip`, checksum e
  `latest.json` con la sola chiave `x3`.
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
* discovery per UUID del servizio, reconnect dell'ultimo peripheral e
  CoreBluetooth state restoration;
* subscribe ad Action Notify, `time.sync` automatico e snapshot
  `priorities.snapshot` multipart entro MTU/limite firmware;
* gestione `priority.toggle` e `priorities.sync.request`;
* coda GATT sequenziale con write-with-response e recovery dopo 8 secondi;
* 4 contract test host per tempo locale, schema snapshot, split ordinato e
  decode toggle.

Hardening firmware v0.2 già applicato e compilato:

* Card Write richiede link cifrato; Action Read/Notify dichiara accesso cifrato;
* la richiesta di resync ANCS resta pendente durante reconnect/discovery e
  ritenta dopo errori di subscribe, senza lasciare il CCCD spento.

Verifica osservata il 13/08/2026:

```text
xcodebuild iPhoneOS unsigned: BUILD SUCCEEDED
swift test: 4 test, 0 failure
Simulator iPhone 17 Pro: install, launch e screenshot riusciti
firmware lume-x3: SUCCESS; RAM 144.988 B; flash 2.526.395 B
firmware.bin: 2.538.928 B
SHA-256: fc4b4bdbc2990a28c1735ca47050be93c8246e11387af1341c0d9fc4fd173378
```

Blocco esterno residuo: il Mac possiede il certificato Apple Development
`PRF667R7JB`, ma Xcode non ha un account registrato né un provisioning profile
per `com.maurizio.lume`. Dopo aver aggiunto l'account in Xcode, la build firmata
può essere installata sull'iPhone 16 Pro collegato; il telefono usa iOS 27.0,
quindi per installazione/debug serve inoltre una versione Xcode che supporti
iOS 27.

## Evidenza di build

Comando eseguito dalla root modulo:

```sh
cd xphone-os
../.venv/bin/pio run -e lume-x3
```

Risultato osservato:

```text
SUCCESS
RAM:   144988 / 327680 bytes (44.2%)
Flash: 2526151 / 6553600 bytes (38.5%)
firmware.bin: 2538688 bytes
SHA-256: 82144a02e2da0b342f9576e30c9367cc2b1af99aabbef0146137bbfcfd7b4a08
```

Confronto col build upstream eseguito immediatamente prima delle modifiche:

| | Upstream dual X3/X4 | Lume X3-only | Delta |
|---|---:|---:|---:|
| RAM PlatformIO | 145.004 B | 144.988 B | −16 B |
| Flash PlatformIO | 2.532.155 B | 2.526.151 B | −6.004 B |
| Total image size report | 2.544.539 B | 2.538.535 B | −6.004 B |

Il delta conferma l'analisi: eliminare X4 semplifica e riduce flash, non libera RAM
significativa perché il framebuffer X3 è già quello più grande. Warning di build
rimasti sono ereditati (SdFat/FS macro, JPEGDEC/PNGdec macro, `NetworkClient::flush`
deprecato), non introdotti dal cutover.

## Prova hardware v0.1 — completata

Il 13/08/2026 PlatformIO ha rilevato l'X3 su `/dev/cu.usbmodem11101`:

```text
USB VID:PID=303A:1001
ESP32-C3 revision v0.4
MAC d4:05:92:91:72:3c
flash auto-rilevata: 16 MB
```

Upload eseguito con `pio run -e lume-x3 -t upload`: 2.538.688 B scritti,
**hash verificato da esptool**, hard reset completato, `SUCCESS` in 37,66 s.
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

## Prossime azioni

1. Installare Xcode con supporto iOS 27 e aggiungere l'Apple Account di Maurizio
   in Xcode Settings > Accounts; lasciare `Automatically manage signing` per il
   team `PRF667R7JB`.
2. Installare Lume sull'iPhone 16 Pro, collegarsi all'X3 e verificare end-to-end:
   bonding, time sync, snapshot iniziale, toggle dall'X3, reconnect e timeout.
3. Ricollegare l'X3 via pogo dati, flashare il firmware hardenizzato e verificare
   che l'app upstream non possa scrivere prima della cifratura ma continui a
   funzionare dopo il bonding; aprire Notifications dopo un reconnect per il
   resync sticky.
4. Creare il repository GitHub personale e aggiungerlo come `origin`, mantenendo
   `upstream` fetch-only.
5. Dopo l'accettazione del vertical slice, implementare Today/EventKit. Non
   anticipare Screen Time: richiede Apple Developer Program a pagamento.

## Decisioni da non riaprire senza nuova evidenza

Nome Lume; X3-only; tutte e sei le app; italiano con language switcher; °C/24h; reader
Wi-Fi dall'app; protocollo compatibile; HTTP senza token finché il transfer è effimero;
rollback OTA rinviato; RTC DS3231 previsto; dashboard da scrivania obiettivo principale.
Dettagli e fonti: [14-decisioni.md](14-decisioni.md).
