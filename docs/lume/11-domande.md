# Domande da chiudere per rendere il progetto "tuo"

Ogni domanda ha la mia raccomandazione. Rispondi anche solo "ok" dove concordi.

## A. Prodotto — cosa vuoi davvero

1. **Quali delle 6 app usi realmente?** (Block, Priorities, Today, Notifications, Read, Workout)
   Tagliarne 2 fa spazio in RAM/flash e semplifica l'app iOS.
   *Racc.: tieni Priorities + Today + Read + Block; Workout e Notifications solo se le usi.*
2. **Serve una scena nuova?** (pomodoro, habits/tracker abitudini, flashcard, "read-to-unblock",
   lista della spesa, timer allenamento…) Quale ti mancherebbe di più?
   *Racc.: una sola, scelta ora, altrimenti diventa un cantiere.*
3. **A cosa serve il device nella tua giornata**: sveglia/orologio da scrivania, lettore,
   o schermo di focus? Cambia la priorità del punto DS3231 (orologio hardware).
   *Racc.: se lo tieni sul comodino/scrivania, l'RTC diventa la feature #1.*
4. **Lingua della UI del device**: italiano o inglese?
   *Racc.: italiano — i font coprono già gli accenti; è la cosa che fa più "mio".*
5. **Formati**: °C, orario 24h, prima giornata della settimana lunedì, unità peso kg?
   *Racc.: tutto sì; il device mostra stringhe già formattate, quindi è lavoro solo lato app.*

## B. App iOS — vincoli pratici

6. **Account developer Apple a pagamento?** Senza, l'app va reinstallata ogni 7 giorni e
   FamilyControls (Screen Time) non è utilizzabile in modo affidabile.
   *Racc.: se hai il paid account, richiedi subito l'entitlement (tempi non tuoi).*
7. **Il blocco delle app ti serve davvero?** È la feature più costosa in permessi Apple.
   Alternative: solo countdown "sull'onore", o blocco parziale via Shortcuts/Focus mode.
   *Racc.: parti senza Screen Time (device utile subito), aggiungilo quando l'entitlement arriva.*
8. **Sorgenti di dati per Today**: solo Calendario+Promemoria iOS, o anche Google Calendar /
   Todoist / Things / Obsidian?
   *Racc.: EventKit e basta per la v1 (zero backend, zero token).*
9. **Priorità: input vocale + parsing "AI" o inserimento manuale?** L'app originale usa
   Apple Intelligence on-device con OpenAI come backup.
   *Racc.: dettatura di sistema + parser a righe; l'AI dopo, è zucchero.*
10. **Vuoi uno storico/dashboard sull'iPhone** (giorni completati, streak, minuti letti) o
    lo stato vive solo nel momento?
    *Racc.: SwiftData locale dal giorno 1, sincronizzazione iCloud mai.*
11. **Compatibilità col protocollo attuale**: la tua app deve parlare anche con firmware
    upstream (utile per testare col device "vergine"), o puoi divergere e pulire il protocollo
    (rinominare i tipi, rendere `state` obbligatorio, aggiungere `card.ack`)?
    *Racc.: resta compatibile per la v1, poi introduci un `schemaVersion: 2` opzionale.*
12. **Nome e identità** del tuo fork (device advertised name, wordmark dello splash, nome app)?
    Serve prima di toccare `CompanionProtocol.h:25` e `art/FloweLogo.h`.

## C. Reader

13. **Come carichi i libri?** SD via lettore di schede, o sync Wi-Fi dall'app (`transfer.wifi`
    + upload HTTP)? Il sync Wi-Fi è la parte più laboriosa della fase 2.
    *Racc.: v1 con SD a mano, sync Wi-Fi in fase 5 — a meno che ti pesi smontare la SD.*
14. **Il server HTTP resta acceso a lungo?** Oggi è senza autenticazione (chiunque sulla rete
    può cancellare i libri o riavviare il device).
    *Racc.: sì al token, in ogni caso.*
15. **Serve la giustificazione/sillabazione italiana** (oggi giustificato senza sillabazione:
    su colonne strette lascia buchi) o va bene così?
    *Racc.: valutare a fine fase 4, con un libro vero in mano.*

## D. Firmware — scelte tecniche

16. **Solo X3 o mantenere anche X4?** Buttare X4 recupera ~4 KB di RAM (framebuffer
    sovradimensionato) e semplifica artwork/profili.
    *Racc.: solo X3 — hai un X3, e il ramo X4 dell'artwork oggi è comunque irraggiungibile.*
17. **Vuoi la CI di release nel tuo fork** (tag → `update.bin` + checksum) o flashi a mano
    da PlatformIO?
    *Racc.: CI sì, ma con i fix P0 #4/#5 (cache e versione dal tag), altrimenti pubblica binari stale.*
18. **Rollback OTA**: implementiamo `PENDING_VERIFY` + promozione al primo boot? Costa poco e
    ti salva da un brick con solo il cavo pogo come via d'uscita.
    *Racc.: sì, prima di qualunque esperimento sul boot.*
19. **Quanto vuoi divergere da upstream?** Fork "vicino" (rebranding + fix, merge periodici)
    o hard fork (rinomini tutto, non guardi più indietro)?
    *Racc.: vicino per 3 mesi — upstream sta ancora aggiungendo cose (X4 parity, stats, reliability).*
20. **Hardware**: hai il cavo pogo 4 pin? Che SD hai dentro (dimensione/classe)? Serve saperlo
    prima di dipendere dal flash USB o dalla cache reader su SD.

## E. Tempo e ambizione

21. **Quanto tempo hai** per la v1: un weekend, due settimane, un mese? Sotto le due
    settimane la scelta obbligata è: app iOS minima (2.1→2.5) + rebranding, niente altro.
22. **Preferisci partire dall'app iOS o dal firmware?** Il device senza app fa solo il reader;
    il firmware senza modifiche funziona già.
    *Racc.: app iOS prima, firmware dopo — il valore è tutto lì.*
