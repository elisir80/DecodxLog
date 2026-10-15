# Gate di rilascio multipiattaforma — audit DXL-007

## Correzione trovata

Il target `tst_qslcardcontroller` dichiarava `QslCardController.cpp`, ma non
includeva `QslCards.cpp` e `QslDesign.cpp`. Il test suite completo falliva in
fase di link, anche se l'applicazione principale compilava. Il target ora usa
gli stessi sorgenti QSL necessari al controller.

## Verifica locale

```text
bash scripts/release-gate.sh build build/DecoDXLog.app/Contents/MacOS/DecoDXLog
```

Risultato:

- controllo versione/documentazione: passato dopo l'allineamento dei riferimenti
  alla versione `1.17.24` (il primo run aveva rilevato `README=1.17.23`);
- build dell'app macOS: passata;
- CTest: 69/69 passati, 0 falliti;
- eseguibile macOS presente e avviabile dal percorso del gate.
- smoke test offscreen `--grab`: passato.

Il nome del file temporaneo dello smoke test usa ora un template portabile
con `XXXXXX` in coda, compatibile con `mktemp` di macOS e Linux.

Il gate blocca quindi un artefatto locale quando build, test o versione
documentale falliscono, verifica anche l'avvio offscreen e la creazione di una
schermata smoke, invece di limitarsi alla compilazione dell'app. La CI
principale include inoltre un job separato per la suite Cloud. Il
Il comando è stato rieseguito il 2026-10-16. Il primo tentativo si è fermato
correttamente per un riferimento README obsoleto; dopo l'allineamento a
`1.17.24`, il gate ha completato build, CTest (69/69) e smoke test senza
errori. Durante la verifica è emerso anche un errore reale di avvio QML:
`ClusterPanel.qml` usava `Note` senza dichiararlo quando veniva caricato
direttamente. Il componente è stato reso locale a `ClusterPanel`; il test
`udppipeline` è poi passato 1/1 e il gate è stato rieseguito con successo.

Il job Cloud separato è presente in `.github/workflows/ci.yml`; la dipendenza
`httpx` usata dal test client FastAPI è ora dichiarata in
`server/requirements.txt`. La suite Cloud è stata eseguita localmente con
Python 3.11: 136 test passati, 3 warning di dipendenze, 0 errori.

Per evitare che un runner assegnato ma bloccato resti indefinitamente in
`in_progress`, i job Windows, Linux e Cloud della CI hanno ora timeout espliciti
di 35, 35 e 20 minuti. Il timeout fa fallire il gate in modo osservabile; non
trasforma un job sospeso in un successo.

## Limiti residui

La verifica locale è stata eseguita su macOS arm64. La matrice GitHub Actions
ha poi verificato i runner Windows, Linux x86_64, Linux aarch64 e macOS Intel;
restano fuori da questa evidenza l'avvio su una macchina pulita e l'accesso al
portachiavi in un ambiente nuovo.

## Verifica GitHub Actions — 16 ottobre 2026

La matrice del fork è stata controllata in sola lettura. Le run concluse con
successo sono:

- CI: `38809473282`;
- Windows x64 (MSYS2 MinGW): `38809549808`;
- Linux x86_64: `38809545397`;
- Linux aarch64: `38809545397`;
- macOS Apple Silicon: `38809540704`;
- macOS Intel: `38809540704`.

Le job risultano tutte `completed/success`. Questa evidenza copre la matrice
dei runner CI, ma non sostituisce l'installazione manuale su una macchina
pulita o la notarizzazione Developer ID di DXL-008.

## Verifica post-fix QML — run 38824727717

La run CI del commit `4bd7f79` (dopo la correzione di `ClusterPanel.qml`) è
terminata con successo il 2026-10-15: Cloud `success`, Linux `success` e
Windows `success`. La durata osservata è stata rispettivamente di circa 0,5,
10 e 12 minuti. Questa è evidenza reale della matrice Windows/Linux/Cloud per
il fix QML; le run successive con i test di idempotenza e i timeout CI sono
ancora in corso.
