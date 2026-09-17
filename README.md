# DecoLog

Il log di stazione della famiglia Decodium. Offline-first, SQLite, stesso tema e
stessa grammatica di finestre di Decodium e DecoRTTY. FT2 è un modo di prima
classe (`MODE=MFSK`, `SUBMODE=FT2`).

Specifica di partenza: [`docs/FASE0_SPEC.md`](docs/FASE0_SPEC.md).
Licenza: GPL-3.0.

## Stato: 0.1.0 — Fase 0 + mockup DecoLog

L'interfaccia segue i mockup "DecoLog Mockups" (finestra principale 1a, Nuovo QSO 1b,
scheda QSO 1c, profili stazione 1d, Setup 1e) nei tre temi di Decodium.

Funziona:

- **Ricezione da Decodium / WSJT-X / JTDX** sul protocollo UDP (porta 2237 di
  default, multicast opzionale). `LoggedADIF` è la fonte primaria; `QSOLogged`
  si usa solo se l'ADIF non arriva entro 1,5 s (oppure come fonte scelta in Setup).
- **Log SQLite** con lo schema di `db/schema.sql`: nomi ADIF, campi di sync,
  stati QSL per servizio in `qsl_status`, campi non mappati in `adif_extra`.
- **Modifica QSO** con storico: ogni salvataggio è una nuova revisione, la
  precedente va in `qso_history` e si può ripristinare; cancellazione morbida.
- **Profili stazione** (`station_profile`), profilo attivo per i nuovi QSO; il primo
  si crea da solo dal nominativo e locatore che Decodium manda nello Status.
- **Duplicati** configurabili (predefiniti ±2 min digitali, ±10 min a mano).
- **Import/Export ADIF** senza perdite, anche degli stati QSL (verificato dai test).
- **Logbook** con filtri a pillole (banda, modo, mese, ricerca), filtri salvati,
  colonne nascondibili, finestra separata ("Pop"), colonne QSL L Q C E.
- **Call info** dal log: worked-before, distanza e azimut dal locatore, ora locale
  approssimata, stato QSL dell'ultimo QSO. **FT2 Award** (DXCC e locatori in FT2,
  conferme LoTW), statistiche per banda e modo, riepilogo QSL, mappa dei locatori.
- **Entità DXCC** dal nominativo con il `cty.csv` di AD1C (incluso, aggiornabile da
  Setup): DXCC, paese, zone e continente sui QSO da Decodium e manuali, "NEW DXCC" e
  "NEW DXCC on <banda>" mentre si lavora, completamento dei QSO già nel log.
- **Credenziali** di Cloud, QRZ.com, QRZ Logbook, LoTW, Club Log, eQSL e HamQTH nel
  portachiavi di sistema (qtkeychain): nel file delle impostazioni resta solo il nome
  utente. Senza qtkeychain i segreti non si salvano affatto.
- **Callbook** QRZ.com (XML) o HamQTH: nome, QTH, locatore, zone, foto, utente LoTW/eQSL
  in Call info; riempie i campi vuoti del Nuovo QSO. Sessione rinnovata da sola,
  risultati tenuti in memoria per un giorno.
- **Award** calcolati dal log: DXCC, FT2 Award, WAZ, WAS, WPX, locatori, IOTA, POTA,
  SOTA, WWFF. Lavorati e confermati per banda, filtri per banda e gruppo di modi,
  conferme accettate a scelta (LoTW, cartolina, eQSL).
- **Backup** notturno con `VACUUM INTO`, copie a rotazione.
- **Tema**: Ocean Blue / Stellar Light / Darkcodium, variant d'accento, densità,
  colori personalizzati.

Non ancora (mostrati come tali nell'interfaccia): upload e
download QSL, DecoLog Cloud e sync.

## Struttura

```
libs/decodium-ui/   tema e componenti comuni (candidato a modulo condiviso)
src/core/           ADIF, bande, database, protocollo UDP — senza GUI
src/app/            controller, modello della tabella e dei profili
qml/DecoLog/        finestra principale
db/schema.sql       schema SQLite v1
resources/cty/      cty.csv di AD1C (country-files.com) e la sua licenza
tests/              Qt Test: adif, protocol, database
tools/udpsend.cpp   finge di essere Decodium, per provare senza radio
scripts/deploy.sh   prepara la cartella distribuibile
```

## Compilare (Windows, MSYS2 MinGW64)

```sh
export PATH=/c/msys64/mingw64/bin:$PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && ctest --output-on-failure
```

Richiede Qt ≥ 6.5 con Quick, QuickControls2, Sql (driver QSQLITE), Network, Test.
Per le credenziali: `pacman -S mingw-w64-x86_64-qtkeychain` (facoltativo: senza, DecoLog
si compila ma non salva password). Nella distribuzione va incluso `libqt6keychain.dll`.

## Cartella distribuibile (Windows)

```sh
scripts/deploy.sh      # compila e prepara dist/: decolog.exe, Qt, QML, TLS, SQLite, qtkeychain
```

La cartella si avvia con doppio clic anche senza MSYS2. La CI la produce a ogni push
come artefatto `decolog-windows-x64`.

## Provare senza radio

```sh
./build/decolog.exe --db prova.sqlite --port 22370
./build/decolog_udpsend.exe --port 22370                       # QSO FT2 come Decodium
./build/decolog_udpsend.exe --port 22370 --call K1AB --mode FT8 --freq 7074000 --only-qsologged
```

Per le schermate e le prove dell'interfaccia: `--import file.adi` importa all'avvio,
`--theme "Stellar Light"` sceglie il tema, `--show new|qso:<id>|profiles|setup:<pagina>`
apre una finestra di dialogo (anche `menu:columns|filters|saved|row`, `tab:<n>`, `pop`),
`--grab file.png` salva la schermata e chiude. Con `QT_QPA_PLATFORM=offscreen` la finestra non
compare sul desktop.

In Decodium: impostare il server UDP sull'indirizzo e la porta di DecoLog.
