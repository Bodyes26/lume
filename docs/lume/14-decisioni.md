# Decisioni prese e vincoli esterni verificati

Registro delle scelte (13 agosto 2026) e delle risposte alle domande di
[11-domande.md](11-domande.md). Serve a non ridiscutere due volte le stesse cose.

## Decisioni

| # | Tema | Decisione |
|---|---|---|
| 1 | App da tenere | tutte e sei; Workout si toglie **solo** se serve RAM (oggi non serve) |
| 2 | Uso primario | ereader + checklist priorità + **dashboard da scrivania** (l'obiettivo grosso) |
| 3 | Lingua | **italiano completo con language switcher** a runtime |
| 4 | Formati | °C, 24h — lavoro lato app: il device mostra stringhe già formattate |
| 5 | Protocollo | base BLE/JSON/Wi-Fi **identica** all'originale; si possono rendere univoci nomi/ID per accoppiare solo la tua app col tuo firmware |
| 6 | Hardware | **solo X3**, X4 droppato |
| 7 | Libri | **sync Wi-Fi dall'app** (non solo SD) |
| 8 | Sicurezza HTTP | nessuna autenticazione per ora: la modalità è effimera e volontaria |
| 9 | Rollback OTA | rinviato: siamo in alpha, si riflasha `update.bin` |
| 10 | Card su link cifrato | **si fa** (fix da 2 punti, nessun costo d'uso: il pairing esiste già) |
| 11 | RTC | si implementa: il DS3231 c'è sull'X3, il "no RTC" del codice era dovuto all'X4 |
| 12 | Tempo | nessuna scadenza; rilasci incrementali |

## Cosa significavano i P0 di sicurezza, in concreto

* **Card non cifrate.** Le caratteristiche GATT non richiedono link cifrato e il parser
  non controlla `isEncrypted()`: chiunque a portata BLE, **senza pairing**, può scrivere
  `transfer.wifi` (SSID+password salvati in NVS) e `transfer.start`. Lo scenario cattivo
  non è "leggere i tuoi dati" ma "far salire il device sulla rete Wi-Fi di un altro e
  poi scaricare/cancellare i libri dal server HTTP". La probabilità è bassa; il fix
  costa 2 righe e **non introduce nessuna gestione di dispositivi autorizzati**: si
  richiede solo che il link sia cifrato, cosa che il tuo iPhone bonded già fa. Cambiare
  iPhone = nuovo pairing, non un reset.
* **HTTP senza autenticazione.** È il secondo anello dello stesso scenario. Deciso:
  accettabile finché la modalità transfer è attiva solo su richiesta e per pochi minuti.
* **Rollback OTA.** Il rischio è "immagine formalmente valida ma che non funziona": oggi
  si esce solo via USB. Hai il cavo pogo 4 pin, quindi la via d'uscita esiste. Rinviato.

## Vincoli Apple — verificati sulla documentazione ufficiale (13/08/2026)

### Account e Screen Time

| Capability | Account gratuito (Personal Team) | ADP 99 $/anno, build di sviluppo | TestFlight / App Store |
|---|---|---|---|
| `com.apple.developer.family-controls` (Screen Time: FamilyControls + ManagedSettings + DeviceActivity) | **NO** — non è nella colonna gratuita della matrice Apple | **SÌ**, subito, senza richiesta | **RICHIESTA** ad Apple, per l'app e per **ogni** estensione Screen Time |
| EventKit (Calendario/Promemoria) | SÌ, solo chiavi privacy | SÌ | SÌ |
| CoreBluetooth central, anche in background | SÌ (`UIBackgroundModes=bluetooth-central`) | SÌ | SÌ |
| Foundation Models (modello base) / Core ML | SÌ | SÌ | SÌ |
| WeatherKit | **NO** | SÌ (500k chiamate/mese incluse) | SÌ |
| Open-Meteo free | SÌ (non commerciale, ≤10k/giorno, CC BY) | SÌ | SÌ solo se resta non commerciale |
| TestFlight come canale | **NO** | — | richiede ADP |

Fonti: [matrice capability iOS](https://developer.apple.com/help/account/reference/supported-capabilities-ios/),
[Configuring Family Controls](https://developer.apple.com/documentation/xcode/configuring-family-controls),
[Requesting the Family Controls entitlement](https://developer.apple.com/documentation/familycontrols/requesting-the-family-controls-entitlement),
[Personal Team: 3 app/device, profili 7 giorni](https://developer.apple.com/help/account/basics/about-your-developer-account/),
[EventKit](https://developer.apple.com/documentation/eventkit/accessing-the-event-store),
[background modes](https://developer.apple.com/documentation/xcode/configuring-background-execution-modes),
[WeatherKit](https://developer.apple.com/weatherkit/),
[Open-Meteo terms](https://open-meteo.com/en/terms).

**Conseguenze operative**
1. Con account gratuito puoi fare **tutto tranne il blocco delle app**: BLE, priorità,
   Today, notifiche, sync libri, voce e AI. Limiti: firma valida 7 giorni, max 3 app per
   device, 10 App ID/settimana.
2. Il blocco Screen Time richiede l'iscrizione a pagamento. Non esistono scorciatoie:
   AltStore/SideStore rifirmano con lo stesso certificato personale e **non possono**
   iniettare un entitlement che Apple non autorizza nel provisioning profile; i
   marketplace alternativi UE e la Web Distribution richiedono comunque la membership
   (e, per operare un marketplace o usare la Web Distribution, requisiti da azienda:
   organizzazione UE, 2 anni di membership, >1M installazioni annue o lettera di credito
   da 1 M€). La fee waiver esiste solo per nonprofit/scuole/enti pubblici.
3. Quindi: **l'app originale riesce a bloccare le app perché è distribuita da un ADP
   pagante via TestFlight** con l'entitlement approvato. Stessa strada per te, quando
   arrivi a v0.8.
4. `.individual` (auto-autorizzazione del proprietario) è la modalità giusta per un'app
   di autocontrollo: non richiede un account bambino, ma non è anti-manomissione — puoi
   sempre revocare l'accesso o cancellare l'app.

### AI e voce in italiano

* **Foundation Models** (iOS 26): italiano supportato dal lancio; on-device, gratuito,
  nessun entitlement per il modello base; serve iPhone 15 Pro/Pro Max o 16+, Apple
  Intelligence attiva (~7 GB), controlli runtime `SystemLanguageModel.availability` e
  `supportsLocale`. Context 4096 token per sessione → una sessione one-shot per parsing.
* **Perché l'app originale capisce solo l'inglese**: non è un limite di Apple. È molto
  probabilmente (a) un recognizer speech fissato su `en-US` e (b) prompt/istruzioni in
  inglese senza il pattern raccomandato da Apple (`The person's locale is it_IT.` +
  `You MUST respond in Italian.`). Entrambe cose che nella tua app si risolvono.
* **Speech**: `SpeechAnalyzer`/`SpeechTranscriber` (iOS 26) supporta Italian (2 regioni),
  interamente on-device, senza il vecchio limite di 1 minuto; il modello si scarica con
  `AssetInventory`. Serve solo `NSMicrophoneUsageDescription`. Fallback:
  `DictationTranscriber`, `SFSpeechRecognizer` (1 min), dettatura di sistema.
* **Parser deterministico** con `NLTagger`/regex per i pattern certi (`5x5`, `3 km`,
  date): più veloce e verificabile dell'AI, e funziona su qualunque iPhone.

## Nome scelto

**Lume** è il nome definitivo del fork, del firmware e della futura app iOS.
Il nome BLE è `Lume X3`. Motivo: corto, leggibile nel wordmark 120×120 1 bpp,
coerente con luce/e-ink e con l'idea di un oggetto che orienta senza distrarre.
Non usare `Flowe`, derivati di `Flow`, né `Quaderno` (marchio Fujitsu per e-paper).
