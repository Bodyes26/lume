# L'app iOS Flowe — ricostruzione da screenshot + evidenze nel firmware

Il codice dell'app iOS **non è pubblico**. Questo capitolo ricostruisce cosa fa, dai
9 screenshot forniti (`IMG_6609`–`IMG_6617`) e dai riferimenti espliciti ai file
Swift lasciati nei commenti del firmware, che sono la prova più forte disponibile
sull'architettura dell'app originale.

Per lo schema esatto dei messaggi vedi [protocollo BLE](03-protocollo-ble.md).

## Riferimenti Swift citati nel firmware

I commenti del firmware citano file e righe dell'app privata: è la mappa dei
moduli che una re-implementazione deve coprire.

| File Swift citato | Ruolo dedotto | Evidenza |
|---|---|---|
| `BluetoothManager.swift` | central CoreBluetooth: scan per **service UUID** (`scanForPeripherals(withServices:)`, riga ~101), dispatch delle notify in `didUpdateValueFor` (i `type` sconosciuti cadono a vuoto senza errore), invio comandi (`sendBlockResult`, riga ~800) | `xphone-os/src/ble/CompanionProtocol.h:20-25`, `src/ble/CompanionBleService.h:120-124`, `src/scenes/BlockScene.h` |
| `PrioritiesManager.swift` | produce la card `priorities.snapshot` (righe 124-146: `priorityItems` come array di 4 elementi `[id, title, note, done]`), gestisce `handleActionPayload` per `priority.toggle` (`id` + `done`) | `src/ble/CompanionProtocol.h:60-68`, `src/ble/CompanionBleService.cpp` (commenti su `priorityItems`) |
| `WorkoutManager.swift` | produce `workout.snapshot` (`workoutItems` = `[id, name, sets, done]`), consuma `workout.set` in modo idempotente | `src/ble/CompanionProtocol.h:70-80` |
| `TodayManager.swift` | produce `today.snapshot` da EventKit (Calendario + Promemoria) | `xphone-os/README.md:27-30` |
| `WeatherProvider.swift` | banda meteo (WeatherKit, fallback Open‑Meteo senza chiave) → due stringhe già formattate (`weather`, `highLow`) | `xphone-os/README.md:27-30` |
| `BlockManager.swift` | preset dei blocchi + shield Screen Time (FamilyControls / ManagedSettings) | `src/scenes/BlockScene.cpp` (commento sul "preset lookup") |

Nota importante: l'app originale è nata per l'**X4** (`X4Companion.xcodeproj`, schema
`X4Companion`, log "X4 action:") e il firmware X3 ne ha mantenuto il protocollo
identico per non rompere la compatibilità.

## Struttura dell'app (dagli screenshot)

Tab bar a 5 voci, con **Today** al centro rialzato: `Read · Priorities · TODAY · Block · Workout`.
Stile: sfondo carta (grigio caldo ~`#EFEEEB`), verde salvia come unico accento,
titoli serif molto grandi (Playfair/New York-like), card bianche con angoli ~20 px
e ombre minime, hamburger in alto a sinistra con **pallino di stato verde =
device connesso**.

### Read (IMG_6609)
- Titolo "Read", sezione `LIBRARY` con "Device checked 17 hours ago".
- Griglia orizzontale di copertine con badge di stato per libro; legenda
  **On device / Phone only / Device only** → l'app tiene un inventario dei libri
  del device e lo confronta con la propria libreria.
- Banner verde "Everything is synced" + riga con la rete Wi‑Fi corrente
  ("Apollo") e "Change".
- Corrispondenza firmware: l'inventario arriva dal device via notify chunkate
  `reader.shelf` (richiesta `reader.shelf.request`), e l'upload dei libri passa
  dal Wi‑Fi: l'app manda le credenziali con la card `transfer.wifi`, chiede
  `transfer.start`, il device apre il server HTTP e risponde con
  `transfer.status` contenente il proprio IP. Vedi
  [reader e file transfer](05-reader-epub.md) e [BLE](03-protocollo-ble.md).

### Priorities (IMG_6610)
- Lista di priorità con checkbox verde e testo barrato quando fatte, campo
  "Add a priority", pulsante **Send to Device**.
- Icona in alto a destra = "Start the Day" (routine mattutina).
- Firmware: card `priorities.snapshot`, max 10 item, testi fino a 120 caratteri,
  split in più parti (`part`/`parts`) perché una singola write GATT sta in ~512 B.

### Block (IMG_6611)
- Stato corrente: "Ready — Deep Work ready · all apps except 17 · 30m".
- Selettore **PRESET**: `Deep Work · Reading · Evening · Workout`, con
  "Edit 17 Allowed Apps" (FamilyActivityPicker).
- CTA "Start 30m Deep Work" + "Test: Start 1 Minute".
- Contatori **Today / Streak / Total** — gli stessi tre campi che il device
  riceve nella card di stato (`blocksToday`, `blockStreak`, `blocksTotal`):
  è il **telefono** a calcolarli, il device li mostra soltanto.

### Workout (IMG_6612)
- Sezione TODAY con "0/0 done" e testo "The device counts sets with its edge buttons".
- "ADD EXERCISE": nome libero (es. "Bench 135lb") + numero di serie (`5x`) con −/+.
- "NAMED DAYS": si salva la giornata come piano riutilizzabile.
- **Send to Device** → `workout.snapshot` (max 8 esercizi, nome ≤48 caratteri).

### Settings (IMG_6613, IMG_6614)
- **Device**: "xphone X3 — Ready to send cards" + Disconnect. Il nome combacia
  con `CompanionProtocol::deviceName()` del firmware.
- **Notifications → Hidden Apps** ("All apps shown"): filtro per bundle id, che
  il device riceve come card `notif.filter` e alimenta con l'elenco delle app
  viste (`notif.apps` / `notif.apps.request`).
- **Morning Routine**: "Run Start the Day Now" — il flusso mattutino parte al
  primo apri dell'app di ogni giorno.
- **AI Priority Parsing**: "On-device Apple Intelligence" attivo, "Use OpenAI as
  backup" (gpt‑5.4‑mini) opzionale con chiave propria.
- **Permissions**: Screen Time, Calendar, Reminders, Location (Weather).
- **Action Log**: log testuale degli eventi BLE. Uno screenshot mostra il JSON
  reale ricevuto dal device:
  `{"schemaVersion":1,"type":"today.sync.request","sequence":1}`
  e la voce **"Today snapshot 168/512 bytes"** → conferma il budget di 512 byte
  per card e il fatto che l'app misura la dimensione prima di scrivere.

### Start the Day (IMG_6615, IMG_6616, IMG_6617)
Wizard mattutino a 3 passi, ciascuno con "Skip":
1. **"What are your priorities today?"** — testo libero o dettatura ("Speak"),
   poi "Generate Priorities" (parsing on-device) oppure "Keep Current List".
2. **"What's your workout today?"** — descrizione libera → "Generate", oppure
   piano/named day; lista esercizi editabile.
3. **Riepilogo** "Good Morning + data": banda meteo ("Overcast 93F, H 94 / L 70"),
   "ON THE CALENDAR" (prossime 24 h), "PRIORITIES", "WORKOUT", e infine
   **"Send to Device & Begin"** che invia in blocco tutte le card.

## Cosa serve replicare, minimo, perché il device sia utile

Ordine di dipendenza (il resto è opzionale):

1. **Pairing + link cifrato** (senza bond ANCS non parte) e `time.sync` subito
   dopo la connessione: il device non ha RTC affidabile dopo il deep sleep, senza
   `time.sync` orologio e attribuzione giornaliera delle statistiche non funzionano.
2. **`priorities.snapshot`** (+ risposta a `priorities.sync.request` e consumo di
   `priority.toggle`).
3. **`block.status`** in risposta a `block.start` / `block.stop` / `block.break`
   + shield Screen Time reali.
4. **`today.snapshot`** (EventKit + meteo) in risposta a `today.sync.request`.
5. **`workout.snapshot`** + consumo di `workout.set`.
6. **Reader sync**: `reader.shelf.request` → inventario, `transfer.wifi` +
   `transfer.start` → upload HTTP degli EPUB.
7. **`notif.filter`** (nice-to-have): le notifiche arrivano via ANCS anche senza
   app aperta, l'app serve solo a filtrarle.

## Vincoli iOS da tenere presenti (non deducibili dagli screenshot, ma decisivi)

- **ANCS**: non serve alcuna entitlement particolare; iOS espone ANCS a un
  accessorio BLE **bonded**. È il device a fare da client ANCS. L'app non deve
  (e non può) inoltrare le notifiche via canale proprio.
- **Screen Time**: `FamilyControls` richiede l'entitlement
  `com.apple.developer.family-controls` — Apple lo concede su richiesta; senza
  approvazione il blocco non è implementabile in modo nativo (alternative:
  solo countdown "onore", o profilo MDM locale).
- **WeatherKit** richiede un capability a pagamento sull'account developer;
  Open‑Meteo è il fallback keyless già usato dall'originale.
- **Background**: `UIBackgroundModes: bluetooth-central` per riconnettere e
  rispondere alle richieste del device quando l'app non è in foreground; le
  richieste `*.sync.request` arrivano al risveglio del device, cioè quasi sempre
  con app in background.
- **Speech**: dettatura con `SFSpeechRecognizer` (o solo il dictation della
  tastiera, che non richiede permessi extra); il parsing "AI" può essere fatto
  con `FoundationModels` (Apple Intelligence, iOS 18.2+/26) o con un semplice
  parser a righe — il device non vede alcuna differenza.
