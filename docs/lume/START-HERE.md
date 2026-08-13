# Lume — punto di ingresso per ogni sessione

Questo è il primo file da leggere quando una nuova sessione non dispone della chat
precedente. Il repository è un fork in lavorazione di `andrewjiang/flowe-os`; il
prodotto scelto si chiama **Lume** e il solo hardware target è **Xteink X3**.

## Regola di ripresa

1. Leggere questo file.
2. Leggere [CURRENT-STATE.md](CURRENT-STATE.md) per sapere cosa è già realmente fatto,
   cosa è verificato e quale lavoro viene dopo.
3. Leggere [14-decisioni.md](14-decisioni.md) prima di proporre cambi di prodotto.
4. Leggere la sezione della versione corrente in
   [10-scaletta-fork.md](10-scaletta-fork.md).
5. Per lavorare sul codice, aprire solo i capitoli tecnici pertinenti dalla tabella
   sotto. Le citazioni `file:riga` si riferiscono al checkout analizzato: se una riga
   non coincide più, cercare il simbolo citato nel sorgente corrente.
6. Al termine di ogni sessione aggiornare **CURRENT-STATE.md** con: commit/base upstream,
   file modificati, comportamento verificato, comandi e risultati, limiti/blocchi,
   prossimo passo atomico. Una fase pianificata non va mai marcata come implementata.

## Verità e precedenza

In caso di conflitto: **codice e output appena verificati** → `CURRENT-STATE.md` →
`14-decisioni.md` → roadmap → capitoli di analisi → README upstream. Le ipotesi sono
marcate `[INFERENZA]`; non trasformarle in fatti senza build o prova hardware.

## Identità e obiettivo

* Nome prodotto/firmware/app: **Lume**; nome BLE previsto: `Lume X3`.
* Hardware: Xteink X3 soltanto. L'X4 viene rimosso con un guard di boot che impedisce
  di inizializzare il pannello sbagliato.
* Uso primario: ereader + Priorities + dashboard da scrivania; si mantengono tutte e
  sei le app finché una misura reale non impone un taglio.
* UI: italiano completo con selettore di lingua; formati °C e 24 ore.
* Architettura prodotto: l'iPhone calcola e sincronizza; l'X3 mostra, raccoglie input e
  legge EPUB. Il protocollo BLE/JSON/Wi-Fi resta compatibile finché una decisione
  registrata non lo cambia.
* Distribuzione iOS iniziale: account Apple gratuito. Screen Time/Block completo viene
  dopo e richiede Apple Developer Program a pagamento.

## Documenti

| Documento | Quando leggerlo |
|---|---|
| [00-panoramica.md](00-panoramica.md) | mappa del repository e sintesi |
| [02-architettura-firmware.md](02-architettura-firmware.md) | boot, task, scene, grafica, memoria |
| [03-protocollo-ble.md](03-protocollo-ble.md) | specifica implementativa dell'app companion |
| [04-app-e-schermate.md](04-app-e-schermate.md) | comportamento delle scene e dei tasti |
| [05-reader-epub.md](05-reader-epub.md) | pipeline EPUB, cache, font, file transfer |
| [06-hardware-power-ota.md](06-hardware-power-ota.md) | pin, pannello, power, NVS, OTA |
| [07-build-ci-e-fork.md](07-build-ci-e-fork.md) | toolchain, build, CI, sincronizzazione upstream |
| [08-app-ios-osservata.md](08-app-ios-osservata.md) | app originale ricostruita dagli screenshot |
| [09-cose-da-sistemare.md](09-cose-da-sistemare.md) | backlog tecnico ordinato per rischio |
| [10-scaletta-fork.md](10-scaletta-fork.md) | roadmap rilasciabile v0.1 → v1.1+ |
| [11-domande.md](11-domande.md) | domande originarie; non riaprire quelle già decise |
| [12-rtc-e-solo-x3.md](12-rtc-e-solo-x3.md) | DS3231 e clean cutover X3-only |
| [13-personalizzazione-e-i18n.md](13-personalizzazione-e-i18n.md) | lingua, asset, font, landscape, dashboard |
| [14-decisioni.md](14-decisioni.md) | decisioni vincolanti e limiti Apple verificati |
| [CURRENT-STATE.md](CURRENT-STATE.md) | stato operativo e handoff tra sessioni |

## Layout repository

* `xphone-os/`: firmware PlatformIO/Arduino ESP32-C3.
* `freeink-sdk/`: SDK hardware/display vendorizzato.
* `ios/`: companion nativo SwiftUI, progetto XcodeGen e contract test SwiftPM.
* `docs/lume/`: memoria tecnica e decisionale del fork; va aggiornata insieme al codice.
* `docs/screens/`: schermate firmware upstream.

## Comandi canonici

Eseguire dalla root del repository salvo diversa indicazione:

```sh
cd xphone-os
../.venv/bin/pio run -e lume-x3
../.venv/bin/pio run -e lume-x3 -t upload
../.venv/bin/pio device monitor --baud 115200
```

```sh
cd ios
export DEVELOPER_DIR=/Applications/Xcode-beta.app/Contents/Developer
xcodegen generate
xcodebuild build -project Lume.xcodeproj -scheme Lume \
  -configuration Debug -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO
swift test
```

Per il device reale il progetto usa il team `XTU68E98BM`; l'iPhone 16 Pro di
Maurizio usa iOS 27. Priorities, Today/EventKit e l'icona aggiornata sono verificati
su hardware con `/Applications/Xcode-beta.app`.

Prima di flashare, verificare che il cavo pogo sia quello dati a 4 pin. Non usare
comandi distruttivi sulla SD e non inizializzare il display finché il guard X3 non ha
confermato l'hardware.
