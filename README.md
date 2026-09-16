# DecoLog

Il log di stazione della famiglia Decodium. Offline-first, SQLite, stesso tema e
stessa grammatica di finestre di Decodium e DecoRTTY. FT2 è un modo di prima
classe (`MODE=MFSK`, `SUBMODE=FT2`).

Specifica di partenza: [`docs/FASE0_SPEC.md`](docs/FASE0_SPEC.md).
Licenza: GPL-3.0.

## Stato: 0.1.0 — prototipo Fase 0

Funziona:

- **Ricezione da Decodium / WSJT-X / JTDX** sul protocollo UDP (porta 2237 di
  default, multicast opzionale). `LoggedADIF` è la fonte primaria; `QSOLogged`
  si usa solo se l'ADIF non arriva entro 1,5 s.
- **Log SQLite** con lo schema di `db/schema.sql`: nomi ADIF, campi di sync
  (`uuid`, `revision`, `dirty`...), campi non mappati in `adif_extra`.
- **Duplicati**: stesso call + banda + modo/submode entro ±2 min (±10 min a mano).
- **Import/Export ADIF** senza perdite (verificato dai test), lunghezze in
  caratteri o in byte UTF-8, fallback Latin-1.
- **Finestra**: scheda nuovo QSO, QSO in arrivo, tabella con filtro, worked-before
  del nominativo in lavoro, registro attività, barra di stato.
- **Tema**: Ocean Blue / Stellar Light / Darkcodium, variant d'accento, densità.

Non ancora: profili stazione nell'interfaccia, modifica/cancellazione QSO,
callbook, QSL, award, cloud, credenziali in keystore.

## Struttura

```
libs/decodium-ui/   tema e componenti comuni (candidato a modulo condiviso)
src/core/           ADIF, bande, database, protocollo UDP — senza GUI
src/app/            controller e modello della tabella
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

In Decodium: impostare il server UDP sull'indirizzo e la porta di DecoLog.
