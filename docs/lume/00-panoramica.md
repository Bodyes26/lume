# Flowe OS — panoramica e mappa dell'analisi

Analisi della repo `github.com/andrewjiang/flowe-os`, ultimo commit upstream
analizzato `3101448` (`fw-v0.5.0`), eseguita prima di creare il fork personale
per **Xteink X3** e riscrivere l'app iOS companion.

> Questo capitolo fotografa l'**upstream** prima delle modifiche. Per il codice
> Lume corrente e le differenze già implementate, leggere
> [CURRENT-STATE.md](CURRENT-STATE.md); in caso di conflitto prevale quello.

## Cos'è, in una riga

Firmware ESP32-C3 (MIT) che trasforma l'e-reader Xteink X3/X4 in un piccolo
device "focus": sei app a schermo e-ink, un link BLE bonded con l'iPhone che
porta priorità/agenda/workout/stato blocco e le notifiche via ANCS, più un
lettore EPUB completo che funziona anche senza telefono.

## Numeri di riferimento

| Voce | Valore |
|---|---|
| SoC | ESP32-C3, 320 KB RAM, **no PSRAM**, flash 16 MB |
| Pannello | X3: UC8253 792×528 (usato in portrait logico 528×792) · X4: SSD1677 800×480 |
| Framebuffer | 52.272 B statici, 1 bpp, single-buffer |
| Refresh | FAST ~450 ms, FULL ~3,2 s, scrub HALF ogni 10 refresh |
| Radio | NimBLE (peripheral + client ANCS); Wi-Fi solo in modalità file transfer, **mai insieme al BLE** |
| Immagine | **una sola** `update.bin` universale X3/X4, device scelto a boot da fingerprint I²C |
| Sorgenti | ~43.500 righe totali; ~14.000 righe di codice proprio in `xphone-os/src` |
| Licenze | MIT (firmware + FreeInk SDK + CrossPoint), vendored: MIT/Apache-2.0/Zlib |

## Struttura della repo

```
flowe-os/
├── xphone-os/            firmware (il codice che conta)
│   ├── src/
│   │   ├── main.cpp      boot in stage + loop + pump BLE/sleep
│   │   ├── Scene.*       scene manager, dirty-flag, soft-key bar, worker di flush
│   │   ├── Gfx.*         blitter 1bpp, rotazione portrait, font UI
│   │   ├── Input.h       task di sampling 5 ms sulle ladder ADC
│   │   ├── ble/          protocollo companion + client ANCS   ← spec per l'app iOS
│   │   ├── scenes/       le 10 scene (6 app + Settings/About/Reader/FileTransfer)
│   │   ├── reader/       motore EPUB (da CrossPoint) + cache SD + statistiche
│   │   ├── net/          server HTTP di trasferimento libri
│   │   ├── art/, fonts/  asset generati (icone, artwork Block, font UI)
│   │   └── *Store.*      store fissi in RAM + persistenza NVS
│   ├── lib/              vendored: expat, uzlib, PNGdec, JPEGDEC, ZipFile, EpdFontCore…
│   ├── tools/            3 script di build (icone e artwork NON sono qui)
│   └── platformio.ini    3 env che compilano lo stesso binario
├── freeink-sdk/          SDK hardware vendorizzato (board, pannelli, SD, input, batteria)
└── .github/workflows/    un solo workflow: release su tag fw-vX.Y.Z
```

## Indice dei capitoli

| File | Contenuto |
|---|---|
| [02-architettura-firmware.md](02-architettura-firmware.md) | boot in stage, task/concorrenza, scene manager, layer grafico, input, budget RAM |
| [03-protocollo-ble.md](03-protocollo-ble.md) | **spec completa** GATT/JSON/ANCS: è il documento da cui si riscrive l'app iOS |
| [04-app-e-schermate.md](04-app-e-schermate.md) | le 10 scene: layout, tasti, store, effetti BLE, schermata di sleep |
| [05-reader-epub.md](05-reader-epub.md) | pipeline EPUB, cache su SD, font, copertine, server HTTP dei libri |
| [06-hardware-power-ota.md](06-hardware-power-ota.md) | pin X3/X4, pannelli e LUT, sleep/wake, tabella NVS, updater SD/OTA, batteria |
| [07-build-ci-e-fork.md](07-build-ci-e-fork.md) | toolchain, flag di build, CI, licenze, **guida pratica al fork** |
| [08-app-ios-osservata.md](08-app-ios-osservata.md) | l'app iOS ricostruita da screenshot + riferimenti Swift nel codice |
| [09-cose-da-sistemare.md](09-cose-da-sistemare.md) | tutti i problemi trovati, ordinati per priorità |
| [10-scaletta-fork.md](10-scaletta-fork.md) | **roadmap versionata** v0.1 → v1.1: cosa fare, in che ordine, con quale prova |
| [11-domande.md](11-domande.md) | le domande poste per definire il progetto (risposte in 14) |
| [12-rtc-e-solo-x3.md](12-rtc-e-solo-x3.md) | fattibilità RTC DS3231 sull'X3 e checklist per droppare l'X4 |
| [13-personalizzazione-e-i18n.md](13-personalizzazione-e-i18n.md) | i18n con switcher, lettura orizzontale, font, asset, modo dashboard |
| [14-decisioni.md](14-decisioni.md) | decisioni prese, vincoli Apple verificati (account/Screen Time/AI), proposte di nome |

## Le tre cose da sapere prima di toccare qualcosa

1. **Il telefono è il cervello, il device è il vetro.** Il firmware non calcola quasi
   niente: riceve card JSON già formattate (testi, orari, meteo, contatori streak) e
   le disegna. Non ha orologio hardware attivo, non ha rete se non in transfer mode,
   non decide i blocchi. Riscrivere l'app iOS significa riscrivere il **90 % del
   prodotto**; il firmware è per lo più presentazione + input + reader.
2. **BLE e Wi-Fi e reader non convivono.** Con BLE+ANCS residenti il device idla a
   ~39 KB di heap libero: il reader sospende il BLE all'ingresso, il file transfer lo
   spegne fino al reboot. Ogni feature nuova va pesata in KB.
3. **Un solo binario per X3 e X4.** Gli env `x3`/`x4`/`xteink` compilano lo stesso
   file; il pannello viene scelto a runtime. Non esistono più build per-device.
