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
- **Backup** notturno con `VACUUM INTO`, copie a rotazione.
- **Tema**: Ocean Blue / Stellar Light / Darkcodium, variant d'accento, densità,
  colori personalizzati.

Non ancora (mostrati come tali nell'interfaccia): callbook QRZ/HamQTH, upload e
download QSL, DecoLog Cloud e sync, credenziali in keystore, award oltre FT2.

## Struttura

```
libs/decodium-ui/   tema e componenti comuni (candidato a modulo condiviso)
src/core/           ADIF, bande, database, protocollo UDP — senza GUI
src/app/            controller, modello della tabella e dei profili
qml/DecoLog/        finestra principale
db/schema.sql       schema SQLite v1
tests/              Qt Test: adif, protocol, database
tools/udpsend.cpp   finge di essere Decodium, per provare senza radio
```

## Compilare (Windows, MSYS2 MinGW64)

```sh
export PATH=/c/msys64/mingw64/bin:$PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && ctest --output-on-failure
```

Richiede Qt ≥ 6.5 con Quick, QuickControls2, Sql (driver QSQLITE), Network, Test.

## Provare senza radio

```sh
./build/decolog.exe --db prova.sqlite --port 22370
./build/decolog_udpsend.exe --port 22370                       # QSO FT2 come Decodium
./build/decolog_udpsend.exe --port 22370 --call K1AB --mode FT8 --freq 7074000 --only-qsologged
```

Per le schermate e le prove dell'interfaccia: `--import file.adi` importa all'avvio,
`--theme "Stellar Light"` sceglie il tema, `--show new|qso:<id>|profiles|setup:<pagina>`
apre una finestra di dialogo.

In Decodium: impostare il server UDP sull'indirizzo e la porta di DecoLog.
