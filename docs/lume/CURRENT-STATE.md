# Lume — stato operativo e handoff

**Aggiornato:** 13 agosto 2026  
**Versione firmware:** `0.1.0-dev`  
**Versione app iOS:** `0.2.0-dev`  
**Base upstream:** `andrewjiang/flowe-os@3101448b02362e627cb17c4de863c1ed22d2478d` (`fw-v0.5.0`)  
**Commit vertical slice v0.2:** `75639a3`  
**Fase:** firmware v0.1 accettato; vertical slice app iOS v0.2 verificato end-to-end su iPhone e X3.

Questo file descrive soltanto lavoro realmente osservato. Per riprendere da una nuova
sessione, partire da [START-HERE.md](START-HERE.md).

## Stato semaforo

| Area | Stato | Evidenza / limite |
|---|---|---|
| Documentazione persistente | **VERIFICATA** | 14 capitoli + START/HANDOFF dentro `docs/lume/` |
| Toolchain locale | **VERIFICATA** | Python 3.11, `.venv`, PlatformIO Core 6.1.19 |
| Build upstream di riferimento | **VERIFICATA** | `xteink` prima delle modifiche: SUCCESS; flash 2.532.155 B, RAM 145.004 B |
| Build Lume X3-only | **VERIFICATA** | ultimo build: SUCCESS; flash 2.527.197 B, RAM 144.996 B |
| Identità a compile time | **VERIFICATA** | stringhe, UUID GATT Lume, identità BLE random-static, asset/env/release compilano |
| Boot e resa sul vetro | **VERIFICATA DALL'UTENTE** | Maurizio ha provato le schermate e confermato il funzionamento complessivo |
| BLE/ANCS reale v0.1 | **VERIFICATA** | nome advertising `Lume X3`; notifica WhatsApp ricevuta e renderizzata |
| App iOS Lume v0.2 | **VERIFICATA SU HARDWARE** | build firmata e installata su iPhone 16 Pro/iOS 27; pairing, `time.sync`, snapshot Priorities e toggle bidirezionale riusciti |

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

## Evidenza di build

Comando eseguito dalla root modulo:

```sh
cd xphone-os
../.venv/bin/pio run -e lume-x3
```

Risultato osservato:

```text
SUCCESS
RAM:   144996 / 327680 bytes (44.2%)
Flash: 2527197 / 6553600 bytes (38.6%)
firmware.bin: 2539728 bytes
SHA-256: 0aa49a6e4e128cdae1b73ab69f3b4c0a9297301576a952760e05ef07c255ba7d
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

Ultimo upload eseguito con `pio run -e lume-x3 -t upload`: immagine da
2.539.728 B, **hash verificato da esptool**, hard reset completato e `SUCCESS`.
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
- [x] vecchia app Flowe isolata: dopo UUID e identità BLE dedicati, il seriale
  mostra un solo snapshot `priorities-sync-…`, senza la seconda card `p-…`.

Il test ha anche riprodotto la collisione originale: prima dell'isolamento, Lume
inviava lo snapshot corretto e Flowe rispondeva subito dopo alla stessa notify,
sovrascrivendolo. Cambiare i soli UUID non bastava per via della cache
CoreBluetooth; la nuova identità random-static risolve anche quel percorso.

## Prossime azioni

1. Verificare reconnect dopo un ciclo completo deep-sleep/wake e il recovery del
   timeout GATT scollegando intenzionalmente il device durante una write.
2. Implementare Today/EventKit come prossimo vertical slice dell'app iOS.
3. Creare il repository GitHub personale e aggiungerlo come `origin`, mantenendo
   `upstream` fetch-only.
4. Non anticipare Screen Time: richiede Apple Developer Program a pagamento.

## Decisioni da non riaprire senza nuova evidenza

Nome Lume; X3-only; tutte e sei le app; italiano con language switcher; °C/24h; reader
Wi-Fi dall'app; payload JSON compatibile ma identità/UUID GATT Lume isolati; HTTP senza
token finché il transfer è effimero; rollback OTA rinviato; RTC DS3231 previsto;
dashboard da scrivania obiettivo principale.
Dettagli e fonti: [14-decisioni.md](14-decisioni.md).
