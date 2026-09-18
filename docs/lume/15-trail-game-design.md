# Trail — gioco narrativo-gestionale su Lume

Specifica di design del sistema Trail: motore narrativo con minigiochi, storie
intercambiabili e integrazione iOS. Questo documento raccoglie le decisioni
prese e serve da guida per la creazione di nuove storie.

**Data:** 19 agosto 2026
**Stato:** design, nessun codice ancora scritto

---

## 1. Visione

Un sistema di giochi narrativo-gestionali in stile Oregon Trail, giocabili
interamente sul device Lume X3 (e-ink 792×528, 1-bit, 6 tasti). Le storie sono
**dati**, non codice: si scrivono, si distribuiscono dall'app iOS e si scaricano
sul device senza mai flashare il firmware.

Ispirazione: Oregon Trail, 80 Days, Reigns, FTL — scelte consequenziali,
gestione di risorse scarse, eventi casuali, minigiochi integrati nella
narrazione.

---

## 2. Architettura a tre strati

```
┌─────────────────────────────────────────────────┐
│  MOTORE  (firmware, compilato in flash)          │  fisso, universale
│  interpreta qualsiasi storia, esegue la micro-VM │  ~25-35 KB codice
├─────────────────────────────────────────────────┤
│  STORIE  (dati, SPIFFS, intercambiabili)         │  scaricate dall'app
│  testo + albero + illustrazioni + minigiochi     │  ~200-800 KB ciascuna
├─────────────────────────────────────────────────┤
│  SALVATAGGI  (NVS, uno per storia)               │  persistenti sempre
│  posizione, risorse, compagni, decisioni prese   │  ~128-256 B ciascuno
└─────────────────────────────────────────────────┘
```

Il motore è al gioco narrativo ciò che il Reader è all'EPUB: un interprete
universale che non sa quale contenuto sta eseguendo.

### Decisioni chiave

| # | Tema | Decisione |
|---|---|---|
| 1 | Dove vive il motore | firmware, `src/games/trail/` — pura logica + scene renderer |
| 2 | Dove vivono le storie | SPIFFS, `/trail/*.story` — dati binari compressi |
| 3 | Dove vivono i salvataggi | NVS, una chiave per storia, ~128-256 B |
| 4 | Distribuzione storie | dall'app iOS via BLE card o HTTP file transfer |
| 5 | Rilascio contenuti | a capitoli (stagioni), più storie in parallelo |
| 6 | Minigiochi | logica in bytecode dentro il file storia, interpretata dalla micro-VM |
| 7 | Prima storia | compilata in flash come i pack Sudoku; le successive via SPIFFS |

---

## 3. Budget hardware

| Risorsa | Totale | Usato (v0.1) | Libero | Note |
|---|---|---|---|---|
| Flash (app slot) | 6.4 MB | 2.5 MB | ~3.8 MB | motore + prima storia qui |
| SPIFFS | 3.4 MB | ~0 | 3.4 MB | 5-8 storie simultanee |
| RAM statica | 327 KB | 145 KB | ~183 KB | VM + stato gioco: <1 KB |
| NVS | 20 KB | parziale | ~10+ KB | ~80-100 salvataggi da 128 B |
| Display | 528×792 px | — | — | 1-bit, portrait logico |
| Input | 6 tasti | — | — | Up/Down/Left/Right/Confirm/Back |

### Stima costi del sistema Trail

| Componente | Flash | RAM statica |
|---|---|---|
| Motore narrativo (parser, albero, risorse) | ~10-15 KB | ~512 B |
| Micro-VM (interprete bytecode) | ~4-6 KB | ~420 B |
| TrailScene (renderer) | ~5-8 KB | ~64 B |
| TrailListScene (catalogo storie) | ~2-3 KB | ~32 B |
| Bitmap decompressor (illustrazioni) | ~2-3 KB | ~2 KB buffer |
| **Totale motore** | **~25-35 KB** | **~3 KB** |

---

## 4. Formato storia (`.story`)

Ogni storia è un singolo file binario compresso su SPIFFS. Il motore lo legge
capitolo per capitolo — in RAM c'è solo il nodo corrente + metadati.

### 4.1 Struttura del file

```
┌─────────────────────────────┐
│ Header (fisso, 64 B)        │  magic, versione, ID, hash
├─────────────────────────────┤
│ Metadati storia             │  titolo, autore, descrizione,
│                             │  numero capitoli, definizione
│                             │  risorse e compagni
├─────────────────────────────┤
│ Indice capitoli             │  offset + dimensione per capitolo
├─────────────────────────────┤
│ Capitolo 1                  │  nodi + stringhe + bytecodes
│   Nodi narrativi            │
│   Minigiochi (bytecode)     │
│   Illustrazioni (bitmap)    │
│   Tabella stringhe          │
├─────────────────────────────┤
│ Capitolo 2                  │
│   ...                       │
├─────────────────────────────┤
│ ...                         │
└─────────────────────────────┘
```

### 4.2 Header

```c
struct StoryHeader {             // 64 byte
    char     magic[4];           // "LTRL"
    uint8_t  version;            // versione formato (1)
    uint8_t  flags;              // riservato
    uint16_t chapterCount;       // numero capitoli
    char     id[24];             // identificativo univoco, es. "silk_road"
    char     title[32];          // titolo visualizzato, es. "La Via della Seta"
};
```

### 4.3 Metadati storia

Definiti una volta per storia, letti al caricamento:

```yaml
meta:
  locale: "it"                   # lingua del testo
  description: "Carovana mercantile da Venezia alla Cina, anno 1271."
  author: "Lume"

  resources:                     # max 8 risorse per storia
    - id: 0
      name: "Provviste"
      icon: bar                  # bar | number | fraction
      max: 100
      start: 80
      critical: 10              # sotto questa soglia, allarme visivo
    - id: 1
      name: "Salute"
      icon: bar
      max: 100
      start: 100
      critical: 20
    - id: 2
      name: "Denaro"
      icon: number
      max: 9999
      start: 200
    - id: 3
      name: "Seta"              # risorsa specifica di questa storia
      icon: number
      max: 50
      start: 0

  companions:                    # max 6 compagni per storia
    - id: 0
      name: "Marco"
      trait: "Esploratore"
    - id: 1
      name: "Fatima"
      trait: "Mercante"
    - id: 2
      name: "Chen"
      trait: "Guardia"
```

### 4.4 Nodo narrativo

Unità base della storia. Ogni capitolo è un grafo di nodi.

```yaml
node:
  id: "river_crossing"
  illustration: 12              # indice bitmap nel capitolo, -1 = nessuna
  title: "Il Guado"             # opzionale, header della scena
  text: >
    Il fiume è in piena. L'acqua arriva al petto dei cavalli.
    Marco suggerisce di aspettare, ma le provviste calano.
    Fatima ha sentito di un ponte a due giorni di marcia verso nord.

  choices:
    - label: "Attraversare ora"
      effects:
        - { resource: 0, delta: -10 }     # provviste -10
        - { resource: 1, delta: -15 }     # salute -15 (rischio)
      next: "crossing_attempt"
      probability: 0.6                     # 60% successo, 40% → nodo fallimento
      fail_next: "crossing_fail"

    - label: "Aspettare un giorno"
      effects:
        - { resource: 0, delta: -8 }      # un giorno di provviste
      condition:
        resource: 0
        min: 8                             # visibile solo se hai provviste
      next: "wait_one_day"

    - label: "Cercare il ponte (2 giorni)"
      effects:
        - { resource: 0, delta: -16 }
      next: "find_bridge"

    - label: "Cacciare prima di decidere"
      minigame: "hunt_river"               # lancia minigioco, poi torna
      next: "river_crossing"               # dopo il minigioco, stesse scelte
```

### 4.5 Tipi di nodo

| Tipo | Scopo | Campi specifici |
|---|---|---|
| `narrative` | testo + scelte (default) | `text`, `choices` |
| `event` | evento casuale, nessuna scelta | `text`, `effects`, `next` |
| `check` | branch condizionale nascosto | `condition`, `pass_next`, `fail_next` |
| `minigame` | lancia un minigioco | `minigame_id`, `win_next`, `lose_next` |
| `shop` | compra/vendi oggetti | `items[]`, `next` |
| `chapter_end` | fine capitolo | `next_chapter` o `story_end` |
| `game_over` | morte/fallimento | `text`, `restart_chapter` o `restart_story` |

### 4.6 Condizioni

Le scelte e i branch possono avere condizioni. Formato:

```yaml
condition:
  # su risorse
  resource: 2        # denaro
  min: 50            # almeno 50

condition:
  # su compagni
  companion: 1       # Fatima
  alive: true        # deve essere nel gruppo

condition:
  # su flag di decisione
  flag: 7            # la decisione #7 è stata presa
  value: true

condition:
  # composta (AND implicito tra le voci)
  all:
    - { resource: 0, min: 20 }
    - { companion: 2, alive: true }
    - { flag: 3, value: true }
```

### 4.7 Effetti

Conseguenze di una scelta. Applicati atomicamente.

```yaml
effects:
  - { resource: 0, delta: -10 }         # provviste -10
  - { resource: 1, set: 100 }           # salute a 100 (fisso)
  - { resource: 2, delta: +50 }         # denaro +50
  - { companion: 1, join: true }        # Fatima si unisce
  - { companion: 2, leave: true }       # Chen se ne va
  - { companion: 0, health: -30 }       # Marco perde 30 salute
  - { flag: 5, set: true }              # segna che hai preso questa decisione
  - { distance: +40 }                   # avanza 40 km
  - { day: +1 }                         # passa un giorno
```

---

## 5. Sistema di risorse

### 5.1 Risorse globali (definite per storia)

Ogni storia definisce fino a **8 risorse** con semantica propria. Il motore le
tratta tutte allo stesso modo: un `int16_t` con min (0), max, valore corrente
e soglia critica. La storia decide cosa significano.

Esempi per storia:

| Via della Seta | Orizzonti Stellari | L'Ultimo Faro |
|---|---|---|
| Provviste | Ossigeno | Cibo |
| Salute | Integrità scafo | Salute |
| Denaro | Energia | Materiali |
| Seta | Dati scientifici | Carburante faro |
| Spezie | Morale equipaggio | Reputazione |

### 5.2 Compagni

Fino a **6 compagni** per storia. Ogni compagno ha:

```c
struct Companion {
    uint8_t  id;              // 0-5
    bool     present;         // nel gruppo o no
    int16_t  health;          // 0 = morto/andato
    uint8_t  trait;           // indice tratto (visuale/narrativo)
};
```

I compagni influenzano la storia: scelte disponibili, eventi, dialoghi,
minigiochi. Un compagno con tratto "Mercante" potrebbe sbloccare la scelta
"Contrattare" in un nodo di commercio.

### 5.3 Flag di decisione

**32 byte = 256 bit** di flag booleane per storia. Ogni scelta importante può
settare un flag che cambia rami futuri. Esempi:

- Flag 0: hai salvato il villaggio al capitolo 1
- Flag 1: hai tradito il mercante
- Flag 2: hai trovato la mappa segreta

I flag sono il meccanismo per le conseguenze a lungo termine. Una decisione
del capitolo 1 può sbloccare (o precludere) una scelta al capitolo 5.

---

## 6. Salvataggi

### 6.1 Formato in NVS

Ogni storia ha una chiave NVS `trail_XXXX` dove `XXXX` sono i primi 4
caratteri dell'ID storia. Il blob è fisso a 128 byte:

```c
struct TrailSave {                  // 128 byte — fisso
    uint32_t storyHash;             // hash dell'ID storia (verifica)
    uint16_t chapter;               // capitolo corrente
    uint16_t nodeId;                // nodo corrente nel capitolo
    uint16_t day;                   // giorno di viaggio
    int16_t  distance;              // distanza percorsa
    int16_t  resources[8];          // valori risorse correnti
    uint8_t  companionState[6];     // present | (health << 1)
    uint8_t  flags[32];             // 256 flag di decisione
    uint8_t  chapterFlags[8];       // capitoli completati (bitmask)
    uint8_t  minigameResults[16];   // esiti minigiochi (per branch futuri)
    uint8_t  padding[14];           // allineamento + espansione futura
    uint16_t checksum;              // CRC-16 dei primi 126 byte
};
```

### 6.2 Quando si salva

- **A ogni cambio nodo** (il giocatore ha fatto una scelta): salvataggio
  automatico, non c'è il concetto di "salva manualmente"
- **Prima del deep sleep** (come i giochi attuali)
- **Mai durante un minigioco** — se il device si spegne durante un minigioco,
  al riavvio si ripresenta lo stesso nodo narrativo che lanciava il minigioco

### 6.3 Indipendenza storia ↔ salvataggio

I salvataggi in NVS **non vengono mai cancellati** quando una storia viene
rimossa da SPIFFS per fare spazio. Quando la storia viene riscaricata, il
salvataggio è ancora lì. Questo permette di avere molte più storie "in corso"
che storie installate.

### 6.4 Sincronizzazione con l'app

A ogni salvataggio, se il BLE è connesso, il device invia:

```json
{
  "type": "trail.save",
  "storyId": "silk_road",
  "chapter": 3,
  "save": "<base64 128 B>"
}
```

L'app tiene un backup di tutti i salvataggi. Factory reset del device → l'app
ripristina i save. Questo è lo stesso pattern del reader (segnalibri).

---

## 7. Micro-VM — minigiochi come dati

### 7.1 Perché un interprete bytecode

Su e-ink con 6 tasti, ogni interazione è **turn-based**: il giocatore preme un
tasto, il gioco elabora, ridisegna. Tra un input e l'altro ci sono centinaia
di millisecondi (tempo umano + refresh e-ink). Il processore a 160 MHz ha
milioni di cicli liberi. Un interprete di bytecode che esegue 200 istruzioni
per gestire una pressione impiega ~10 µs. Invisibile.

Questo rende possibile ciò che su un gioco a 60 fps non lo sarebbe: **la logica
del minigioco è un programma interpretato, non codice compilato**. Il firmware
contiene solo l'interprete; ogni minigioco è bytecode dentro il file `.story`.

### 7.2 Stato della VM

Tutto in BSS statico, zero allocazioni heap:

```c
struct MiniVM {
    // Griglia di gioco: ogni cella è un byte (0-255 stati diversi)
    uint8_t  grid[16][16];       // 256 B — qualsiasi minigioco
    uint8_t  gridW, gridH;       // dimensioni effettive (1-16)

    // Cursore del giocatore sulla griglia
    uint8_t  curX, curY;

    // 16 variabili generiche: il bytecode decide il significato
    // (contatori, punteggio, turni rimasti, stato interno)
    int16_t  vars[16];           // 32 B

    // Stack di esecuzione
    int16_t  stack[32];          // 64 B
    uint8_t  sp;

    // Risultato
    uint8_t  state;              // 0 = playing, 1 = won, 2 = lost
    int16_t  score;              // valore ritornato alla storia

    // Mappa di rendering: cella valore → aspetto visivo
    char     cellChars[16];      // carattere per tipo di cella
    uint8_t  cellStyles[16];     // 0=bianco 1=nero 2=invertito 3=bordo

    // Puntatori nel file storia (non posseduti)
    const uint8_t* pgSetup;     // bytecode inizializzazione
    const uint8_t* pgInput[6];  // bytecode per ogni tasto
    const uint8_t* pgRender;    // bytecode layout (opzionale)
    const char**   strings;     // tabella stringhe del minigioco
    uint8_t        stringCount;
};
```

**Totale: ~420 byte di RAM statica.**

### 7.3 Set di istruzioni

~40 opcodes, codifica a 1 byte + operandi inline dove serve:

```
══════════════════════════════════════════════════════
  STACK
══════════════════════════════════════════════════════
0x01  PUSH imm16     push valore immediato a 16 bit
0x02  POP            scarta il top
0x03  DUP            duplica il top
0x04  SWAP           scambia i due top
0x05  OVER           copia il secondo sotto il top

══════════════════════════════════════════════════════
  ARITMETICA   (pop 2, push risultato)
══════════════════════════════════════════════════════
0x10  ADD
0x11  SUB            a - b (a push per primo, b per secondo)
0x12  MUL
0x13  DIV            divisione intera, div by zero → 0
0x14  MOD            modulo, mod by zero → 0
0x15  NEG            pop 1, push -a
0x16  ABS            pop 1, push |a|
0x17  MIN            pop 2, push il più piccolo
0x18  MAX            pop 2, push il più grande
0x19  CLAMP          pop 3 (val, lo, hi), push clamp(val, lo, hi)

══════════════════════════════════════════════════════
  CONFRONTO    (pop 2, push 1 o 0)
══════════════════════════════════════════════════════
0x20  EQ
0x21  NE
0x22  LT
0x23  GT
0x24  LE
0x25  GE

══════════════════════════════════════════════════════
  LOGICA       (pop 1 o 2, push 1 o 0)
══════════════════════════════════════════════════════
0x28  AND
0x29  OR
0x2A  NOT

══════════════════════════════════════════════════════
  CONTROLLO DI FLUSSO
══════════════════════════════════════════════════════
0x30  JMP off16      salto relativo (signed)
0x31  JZ off16       salta se top == 0 (consuma il top)
0x32  JNZ off16      salta se top != 0 (consuma il top)
0x33  CALL off16     push indirizzo di ritorno, salta
0x34  RET            pop indirizzo, salta
0x35  HALT           termina esecuzione del programma corrente

══════════════════════════════════════════════════════
  VARIABILI
══════════════════════════════════════════════════════
0x40  LOAD id8       push vars[id]
0x41  STORE id8      pop → vars[id]
0x42  INC id8        vars[id]++
0x43  DEC id8        vars[id]--, clamp a 0

══════════════════════════════════════════════════════
  GRIGLIA
══════════════════════════════════════════════════════
0x50  GINIT w8 h8    inizializza griglia w×h, riempita di 0
0x51  GSET           pop (val, y, x) → grid[y][x] = val
0x52  GGET           pop (y, x) → push grid[y][x]
0x53  GFILL val8     riempi tutta la griglia con val
0x54  GCOUNT val8    push quante celle hanno val
0x55  GADJ           pop (y, x) → push conteggio 8-adiacenti != 0
0x56  GADJ4          pop (y, x) → push conteggio 4-adiacenti != 0
0x57  GFLOOD         pop (new, old, y, x) → flood fill
0x58  GSWAP          pop (y2, x2, y1, x1) → scambia due celle
0x59  GROW           pop (y, x) → push valore + riga intera (per check linee)

══════════════════════════════════════════════════════
  CURSORE
══════════════════════════════════════════════════════
0x60  CURX           push curX
0x61  CURY           push curY
0x62  CMOV           pop (y, x) → muovi cursore, clamp ai bordi
0x63  CREL           pop (dy, dx) → muovi relativo, clamp

══════════════════════════════════════════════════════
  RANDOM
══════════════════════════════════════════════════════
0x68  RAND           pop max → push random [0, max)

══════════════════════════════════════════════════════
  RENDERING
══════════════════════════════════════════════════════
0x70  CMAP id8 ch8   cellChars[id] = ch (carattere ASCII/simbolo)
0x71  CSTYLE id8 s8  cellStyles[id] = s
0x72  TEXT id8       mostra strings[id] nell'area testo
0x73  TITLE id8      imposta titolo da strings[id]
0x74  BAR id8        mostra barra per vars[id], max = vars[id+1]
0x75  SCORE          mostra punteggio = top dello stack
0x76  FLASH          richiede refresh parziale (feedback visivo)

══════════════════════════════════════════════════════
  RISULTATO
══════════════════════════════════════════════════════
0x80  WIN            stato = won, score = top dello stack
0x81  LOSE           stato = lost
0x82  YIELD          ritorna valore alla storia senza terminare
```

### 7.4 Sicurezza e limiti

L'interprete è **sandboxed** per design:

- **Stack overflow/underflow**: check a ogni push/pop; violazione → HALT + LOSE
- **Accesso griglia fuori limite**: clamp silenzioso (GGET ritorna 0)
- **Variabile fuori range**: id & 0x0F — sempre valido
- **Jump fuori programma**: HALT + LOSE
- **Loop infinito**: contatore di istruzioni, max 10.000 per invocazione;
  superato → HALT + LOSE. Più che sufficiente per qualsiasi logica di gioco,
  impossibile bloccare il device.
- **Nessun accesso a memoria, periferiche, NVS, BLE**: la VM vede solo la sua
  griglia, le sue variabili e il suo stack.

### 7.5 Come il renderer usa lo stato della VM

Il firmware (non la VM) disegna lo schermo leggendo lo stato dopo l'esecuzione:

```
┌──────────────────────────────────────┐
│ [titolo dal TITLE opcode]        ⊙   │ header
├──────────────────────────────────────┤
│                                      │
│   ·  ·  ~  ~  ·  ~  ~  ·           │ griglia:
│   ~  ·  ·  ✦  ·  ~  ·  ~           │ cellChars[grid[y][x]]
│   ·  ~  ·  ·  ·  ·  ·  ·           │ con cursore evidenziato
│   ~  ·  ~  ·  ·  ~  ·  ·           │
│   ·  ·  ·  ~  ·  ·  ×  ·           │
│   ~  ~  ·  ·  ~  ·  ·  ~           │
│                                      │
├──────────────────────────────────────┤
│ Lenze: ███░░ 3/5                     │ BAR: vars[slot]
│ Pesci: ██░░░ 2                       │
├──────────────────────────────────────┤
│ Pesce!                               │ TEXT: stringa corrente
├──────────────────────────────────────┤
│ ESCI       OK       ◂       ▸       │ soft-key
└──────────────────────────────────────┘
```

La VM non chiama mai `Gfx` direttamente. Setta stato → il renderer nativo lo
dipinge con la qualità grafica del firmware (font, antialiasing, spaziature
coerenti con il resto di Lume).

---

## 8. Minigiochi — catalogo meccaniche

Ogni meccanica è una combinazione di griglia + variabili + bytecode.
Non sono "giochi fissi nel firmware" ma **pattern** che il bytecode può
realizzare in modi diversi.

### 8.1 Meccaniche base

| Meccanica | Griglia | Input principale | Esempi |
|---|---|---|---|
| **Cerca e rivela** | celle nascoste, reveal al confirm | cursore + confirm | caccia, pesca, scansione, scavo |
| **Ruota/posiziona** | celle con orientamento, ruotano al confirm | cursore + confirm | circuiti, ingranaggi, serrature |
| **Naviga** | griglia con ostacoli, il giocatore si muove | frecce = movimento | tempesta, labirinto, asteroidi, rovine |
| **Sequenza** | la griglia mostra un pattern, il giocatore riproduce | frecce in ordine | segnali, lingua aliena, codice |
| **Scelta pesata** | nessuna griglia, lista di opzioni con valori nascosti | up/down + confirm | contrattazione, baratto, allocazione |
| **Incastro** | pezzi da posizionare nello spazio | frecce + confirm per piazzare | carico, riparazione, costruzione |
| **Fog of war** | griglia parzialmente visibile, si rivela muovendosi | frecce = esplora | rovine, esplorazione, ricognizione |

### 8.2 Combinazioni

I minigiochi interessanti nascono combinando meccaniche:

- **Pesca** = cerca e rivela + target mobili (i pesci si muovono ogni turno)
- **Esplorazione rovine** = fog of war + cerca e rivela (trovi oggetti ma rischi crolli)
- **Riparazione** = ruota/posiziona + condizione di vittoria a flusso (connetti A a B)
- **Contrattazione** = scelta pesata + bluff (l'AI del mercante reagisce)

### 8.3 Come definire un minigioco nel file storia

```yaml
minigames:
  hunt_river:
    title: "Caccia al fiume"
    grid: { w: 8, h: 6 }

    cell_map:
      0: { char: "·", style: white }    # terreno vuoto
      1: { char: "♠", style: black }    # cespuglio
      2: { char: "·", style: white }    # preda nascosta (sembra vuoto)
      3: { char: "◆", style: black }    # preda colpita
      4: { char: "×", style: black }    # colpo a vuoto

    vars:
      0: colpi_rimasti     # inizializzato a 6
      1: prede_colpite     # inizializzato a 0
      2: prede_totali      # inizializzato a 5

    strings:
      - "Colpita!"           # 0
      - "A vuoto..."         # 1
      - "Colpi esauriti"     # 2
      - "Caccia finita!"     # 3

    setup: <bytecode>         # piazza cespugli e prede nascoste
    on_up: <bytecode>         # CURY PUSH 1 SUB CURX SWAP CMOV
    on_down: <bytecode>       # CURY PUSH 1 ADD CURX SWAP CMOV
    on_left: <bytecode>       # CURX PUSH 1 SUB CURY CMOV
    on_right: <bytecode>      # CURX PUSH 1 ADD CURY CMOV
    on_confirm: <bytecode>    # logica sparo + check fine
    on_back: <bytecode>       # LOSE (rinuncia)
```

Il bytecode di `on_confirm` per questo esempio:

```
# Pseudoassembly (compilato in bytecode binario)

    LOAD 0              # colpi rimasti
    PUSH 0
    LE                  # <= 0?
    JNZ @end            # sì → fine

    CURX                # leggi cella sotto cursore
    CURY
    GGET
    DUP

    PUSH 2              # è una preda nascosta (tipo 2)?
    EQ
    JZ @miss

    # --- colpita ---
    POP                 # butta il valore letto
    PUSH 3              # tipo 3 = preda colpita ◆
    CURX
    CURY
    GSET                # aggiorna cella
    INC 1               # prede_colpite++
    TEXT 0              # "Colpita!"
    JMP @spent

@miss:
    POP
    PUSH 4              # tipo 4 = colpo a vuoto ×
    CURX
    CURY
    GSET
    TEXT 1              # "A vuoto..."

@spent:
    DEC 0               # colpi_rimasti--

    # check fine gioco
    LOAD 0              # colpi rimasti
    PUSH 0
    LE                  # esauriti?
    JNZ @game_over

    HALT                # continua a giocare

@game_over:
    LOAD 1              # prede colpite
    PUSH 2
    GE                  # >= 2?
    JZ @lose

    LOAD 1
    WIN                 # vinto! score = prede colpite

@lose:
    LOSE

@end:
    HALT
```

Questo bytecode pesa **~80 byte**. Con i 6 handler (4 direzioni + confirm +
back), la tabella stringhe e la cell map, l'intero minigioco sta in
**~300-400 byte** di dati.

---

## 9. Illustrazioni

### 9.1 Formato

Bitmap 1-bit compressi con RLE. Dimensione massima: 528×300 px (metà superiore
dello schermo — la metà inferiore è testo + risorse + scelte).

| Dimensione raw | Compressa (RLE, immagini semplici) | Per storia |
|---|---|---|
| 528×300 = 19.800 B | ~2-8 KB tipico | 30-50 illustrazioni |
| **Totale per storia** | | **~60-250 KB** |

### 9.2 Stile

Le illustrazioni sono pensate per e-ink 1-bit: bianco e nero puro, tratto
netto, alto contrasto. Funzionano bene:

- Woodcut / xilografia (stile incisione storica)
- Pixel art ingrandita (stile retro)
- Silhouette con dettagli interni
- Schemi e mappe stilizzate
- Dithering ordinato per mezzitoni

### 9.3 Generazione

Le illustrazioni possono essere:
- Disegnate a mano e convertite con un tool (soglia + RLE)
- Generate da AI e post-processate (downscale → dither → RLE)
- Procedurali semplici (linee, pattern — codificate come bytecode di disegno)

Un tool di conversione `tools/lume-trail/img2story.py` converte PNG → bitmap
RLE nel formato del file `.story`.

---

## 10. Layout su schermo

### 10.1 Scena narrativa

```
┌──────────────────────────────────────┐
│           [illustrazione]            │  0-300 px
│         o area vuota se assente      │
├──────────────────────────────────────┤
│ Giorno 47 — Valle del Serpente       │  titolo (opzionale)
│ Provviste ██████░░░░ 62  $142       │  risorse (1-2 righe)
│ Salute   █████████░ 88              │
├──────────────────────────────────────┤
│                                      │
│ Il fiume è in piena. Marco           │  testo narrativo (wrap)
│ suggerisce di aspettare, ma          │
│ le provviste calano. Fatima          │
│ ha sentito di un ponte...            │
│                                      │
│ ▸ Attraversare ora                   │  scelte (max 4)
│   Aspettare un giorno                │
│   Cercare il ponte (2 giorni)        │
│   Cacciare prima di decidere         │
│                                      │
├──────────────────────────────────────┤
│ INDIETRO     OK        ▲       ▼    │  soft-key bar (44 px)
└──────────────────────────────────────┘
```

### 10.2 Scena minigioco

```
┌──────────────────────────────────────┐
│ Caccia al fiume                  ⊙   │  titolo
├──────────────────────────────────────┤
│                                      │
│    ·  ♠  ·  ·  ·  ♠  ·  ·          │  griglia
│    ·  ·  ♠  ·  ·  ·  ·  ♠          │
│    ♠  ·  ·  ·  ·  ·  ♠  ·          │
│    ·  ·  ·[·] ·  ♠  ·  ·          │  [·] = cursore
│    ·  ♠  ·  ·  ·  ·  ·  ·          │
│    ·  ·  ·  ♠  ·  ·  ♠  ·          │
│                                      │
├──────────────────────────────────────┤
│ Colpi ████░░ 4/6   Prede: 1         │  barre/contatori
├──────────────────────────────────────┤
│ Colpita!                             │  messaggio
├──────────────────────────────────────┤
│ ESCI       OK       ◂       ▸       │  soft-key
└──────────────────────────────────────┘
```

### 10.3 Lista storie

```
┌──────────────────────────────────────┐
│ Avventure                        ⊙   │
├──────────────────────────────────────┤
│                                      │
│ ▸ La Via della Seta        Cap 3/8   │
│   Frontiera Occidentale   Cap 1/6   │
│   Orizzonti Stellari      ☁ 0/12   │
│                                      │
│   Altre storie sull'app Lume         │
│                                      │
├──────────────────────────────────────┤
│ INDIETRO     OK                      │
└──────────────────────────────────────┘
```

Convenzioni:
- `Cap 3/8` → storia in corso, capitolo 3 di 8, presente su SPIFFS
- `☁ 0/12` → salvataggio in NVS ma file non su SPIFFS (serve download)
- `✓ 8/8` → storia completata

---

## 11. Integrazione iOS

### 11.1 Protocollo BLE

Nuove card/action nel protocollo esistente (§3 del doc BLE):

**Device → App (Action Notify):**

```json
{"type":"trail.save","storyId":"silk_road","chapter":3,"save":"<base64>"}
```
```json
{"type":"trail.request","storyId":"stellar_horizons","action":"download"}
```
```json
{"type":"trail.catalog.request"}
```

**App → Device (Card Write):**

```json
{"type":"trail.catalog","stories":[
  {"id":"silk_road","title":"La Via della Seta","chapters":8,"size":420000,"locale":"it"},
  {"id":"frontier","title":"Frontiera Occidentale","chapters":6,"size":380000,"locale":"it"},
  {"id":"stellar","title":"Orizzonti Stellari","chapters":12,"size":650000,"locale":"it"}
]}
```
```json
{"type":"trail.save.restore","storyId":"silk_road","save":"<base64>"}
```

I file `.story` si trasferiscono via HTTP su `lume.local` (lo stesso canale
degli EPUB), non via BLE — troppo lenti per centinaia di KB.

### 11.2 Flusso download

1. Il giocatore seleziona una storia non in SPIFFS (icona ☁)
2. Il device manda `trail.request` via BLE
3. L'app riceve, avvia il server HTTP di trasferimento se non attivo
4. L'app manda `trail.transfer` con l'URL del file
5. Il device scarica via HTTP, salva in SPIFFS, apre la storia
6. Trasparente per l'utente: vede solo "Scaricando..." per qualche secondo

### 11.3 App — sezione Avventure

L'app iOS mostra:
- Catalogo storie disponibili (bundled + scaricabili)
- Stato di ogni storia (non iniziata / in corso / completata)
- Gestione spazio (quale storia è su SPIFFS, quale togliere)
- Backup salvataggi (automatico, in locale/iCloud)

Il gioco si gioca **solo sul device**. L'app è catalogo + gestore + backup.

---

## 12. Creare una nuova storia — checklist

### 12.1 Cosa serve

1. **Trama**: arco narrativo con inizio, sviluppo, finale. Divisibile in
   capitoli (5-15 consigliati). Ogni capitolo: 15-40 nodi narrativi.
2. **Risorse**: max 8, con nomi e bilanciamento. Definire consumo giornaliero,
   soglie critiche, modi di recupero.
3. **Compagni**: max 6, con nomi, tratti e ruolo narrativo. Definire come/quando
   si uniscono o se ne vanno.
4. **Mappa delle decisioni**: quali scelte hanno conseguenze a lungo termine
   (flag). Massimo 256 flag — più che sufficienti.
5. **Minigiochi**: 3-8 per storia, definiti come bytecode. Possono riusare le
   stesse meccaniche base con varianti (una caccia nel bosco e una caccia in
   mare sono la stessa meccanica, ambientazione diversa).
6. **Illustrazioni**: 1-bit, 528×300 max, in stile coerente per tutta la storia.
   Le scene chiave (arrivo in una città, evento drammatico, finale) meritano
   un'illustrazione; i nodi minori no.
7. **Testi**: in una sola lingua per file `.story`. Storie multilingua =
   file `.story` separati con lo stesso ID + suffisso locale.

### 12.2 Formato sorgente

Le storie si scrivono in YAML leggibile (vedi §4 e §8.3 per la struttura),
poi un tool di build le compila nel formato binario `.story`:

```bash
python3 tools/lume-trail/build_story.py \
  --input stories/silk_road/story.yaml \
  --images stories/silk_road/images/ \
  --output silk_road.story
```

Il tool:
- Valida la struttura (nodi irraggiungibili, risorse fuori range, flag non usati)
- Compila il bytecode dei minigiochi dall'assembly testuale
- Converte e comprime le illustrazioni PNG → RLE 1-bit
- Produce il file `.story` binario pronto per SPIFFS

### 12.3 Vincoli di design

| Vincolo | Limite | Motivo |
|---|---|---|
| Risorse per storia | max 8 | salvatagio NVS 128 B |
| Compagni per storia | max 6 | salvatagio NVS |
| Flag di decisione | max 256 | 32 byte nel save |
| Scelte per nodo | max 4 | 4 righe visibili + soft-key |
| Griglia minigioco | max 16×16 | RAM VM (256 B) |
| Variabili VM | max 16 | RAM VM (32 B) |
| Tipi cella minigioco | max 16 | cell map (16 B) |
| Dimensione file storia | consigliato <800 KB | SPIFFS budget |
| Capitoli per storia | max 255 | uint8 nell'header |
| Nodi per capitolo | max 65535 | uint16 nell'indice |
| Testo per nodo | max ~1 KB | buffer RAM lettura |
| Illustrazioni | 528×300, 1-bit | metà schermo, e-ink |
| Stringhe minigioco | max 16 | indice 4-bit |

### 12.4 Consigli di bilanciamento

- **Provviste**: consumo ~5-10/giorno, recupero ~20-30 da caccia/commercio.
  Il giocatore dovrebbe sentire la scarsità ma non morire ogni 3 nodi.
- **Salute**: scende per eventi (malattia -20, ferita -30), sale con riposo
  (+10/giorno di sosta) o medicina (+20). Non deve essere un timer: deve
  riflettere le conseguenze delle scelte.
- **Denaro**: economia chiusa. Entrate: commercio, lavori, minigiochi.
  Uscite: provviste, medicine, pedaggi, imprevisti.
- **Minigiochi**: non più di 1 per ogni 4-5 nodi narrativi. Devono essere
  opzionali dove possibile (la scelta "cacciare" è un'alternativa a
  "comprare cibo", non un obbligo).
- **Difficoltà**: le prime scelte di ogni capitolo dovrebbero avere conseguenze
  leggere. Le scelte pesanti arrivano a metà/fine capitolo, quando il
  giocatore ha contesto.
- **Morte**: possibile ma mai improvvisa. Il giocatore deve vedere le risorse
  calare e avere almeno 2-3 opportunità di correggere prima del game over.
  La morte deve sentirsi come una conseguenza di scelte, non di sfortuna.

---

## 13. Idee per le prime storie

Tre ambientazioni diverse per validare che il motore è davvero universale:

### 13.1 "La Via della Seta"
Carovana mercantile da Venezia alla Cina, 1271. Commercio, sopravvivenza,
diplomazia attraverso deserti, montagne e città lungo la Via della Seta.
**Risorse**: Provviste, Salute, Denaro, Seta, Spezie, Morale.
**Tono**: avventura storica, scelte commerciali e diplomatiche.

### 13.2 "Orizzonti Stellari"
Nave generazionale verso Kepler-442b. Gestione della nave, crisi a bordo,
esplorazione di pianeti intermedi, decisioni sul destino dell'equipaggio.
**Risorse**: Ossigeno, Integrità Scafo, Energia, Dati Scientifici, Morale.
**Tono**: fantascienza hard, dilemmi etici, scoperta.

### 13.3 "L'Ultimo Faro"
Guardiano di un faro costiero in un mondo post-apocalittico. Sopravvivenza
quotidiana, incontri con viandanti e naufraghi, scelte morali su chi aiutare,
ricostruzione graduale di una comunità.
**Risorse**: Cibo, Salute, Materiali, Carburante Faro, Reputazione, Rifugiati.
**Tono**: intimo, survival, scelte morali senza risposta giusta.
