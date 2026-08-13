# Lume — stato operativo e handoff

**Aggiornato:** 13 agosto 2026  
**Versione in sviluppo:** `0.1.0-dev`  
**Base upstream:** `andrewjiang/flowe-os@3101448b02362e627cb17c4de863c1ed22d2478d` (`fw-v0.5.0`)  
**Fase:** v0.1 flashata su X3; boot/launcher e radio osservati, checklist hardware restante ancora aperta.

Questo file descrive soltanto lavoro realmente osservato. Per riprendere da una nuova
sessione, partire da [START-HERE.md](START-HERE.md).

## Stato semaforo

| Area | Stato | Evidenza / limite |
|---|---|---|
| Documentazione persistente | **VERIFICATA** | 14 capitoli + START/HANDOFF dentro `docs/lume/` |
| Toolchain locale | **VERIFICATA** | Python 3.11, `.venv`, PlatformIO Core 6.1.19 |
| Build upstream di riferimento | **VERIFICATA** | `xteink` prima delle modifiche: SUCCESS; flash 2.532.155 B, RAM 145.004 B |
| Build Lume X3-only | **VERIFICATA** | `lume-x3`: SUCCESS; flash 2.526.151 B, RAM 144.988 B |
| Identità a compile time | **VERIFICATA** | stringhe, UUID preservati, asset/env/release compilano |
| Boot e resa sul vetro | **PARZIALE, POSITIVA** | flash verificato; Maurizio ha osservato `lume` nel lockup superiore |
| BLE/ANCS reale | **PARZIALE, POSITIVA** | il monitor ha ricevuto e renderizzato una notifica WhatsApp; nome advertising non ancora scandito |
| App iOS Lume | **NON INIZIATA** | è v0.2; l'app originale non è nel repository |

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
* `README.md` e `xphone-os/README.md` distinguono Lume dall'upstream, dichiarano che
  l'app iOS non esiste ancora e riportano i comandi reali.

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

## Prova hardware v0.1 — eseguita, completamento parziale

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

Checklist di accettazione:

- [x] immagine Lume caricata e hash del flash verificato;
- [x] firmware vivo sul pannello; launcher mostra il lockup `lume`;
- [x] BLE/ANCS operativo: notifica WhatsApp ricevuta, attributi risolti e render eseguito;
- [ ] catturare un boot completo con `[lume] boot: Xteink X3 confirmed` (il monitor è partito dopo il boot);
- [ ] verificare splash/mark senza artefatti o tagli;
- [ ] Settings mostra `Lume 0.1.0-dev (X3)`;
- [ ] About mostra `About Lume` e pannello `xteink_x3` 528×792 logici;
- [ ] sleep senza priorità mostra `lume`; dormant priorities mostra `lume` nel footer;
- [ ] scan Bluetooth vede il nome advertising `Lume X3`;
- [ ] reader apre un EPUB già presente e gira almeno una pagina;
- [ ] riavvio e wake non perdono settings/posizione (namespace NVS preservato).

Non pubblicare ancora la release: il flash è riuscito, ma la checklist delle
schermate e della persistenza va completata.

## Dopo la prova hardware

1. Creare il repository GitHub personale (nome consigliato `lume`) e aggiungerlo:
   `git remote add origin <URL>`; mantenere `upstream` fetch-only.
2. Fare il primo tag soltanto dopo hardware pass; per sviluppo usare `0.1.0-dev`, per
   release `fw-v0.1.0` e versione sorgente `0.1.0`.
3. Iniziare v0.2 dall'app iOS minima: CoreBluetooth, bonding, subscribe Action Notify,
   `time.sync`, Priorities snapshot/toggle. Specifica: `03-protocollo-ble.md`.
4. Nello stesso v0.2 applicare il gate BLE cifrato e i due fix già decisi
   (ANCS resync e timeout toggle). Non anticipare Screen Time: richiede ADP a pagamento.

## Decisioni da non riaprire senza nuova evidenza

Nome Lume; X3-only; tutte e sei le app; italiano con language switcher; °C/24h; reader
Wi-Fi dall'app; protocollo compatibile; HTTP senza token finché il transfer è effimero;
rollback OTA rinviato; RTC DS3231 previsto; dashboard da scrivania obiettivo principale.
Dettagli e fonti: [14-decisioni.md](14-decisioni.md).
