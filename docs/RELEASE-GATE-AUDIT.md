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

- controllo versione/documentazione: passato;
- build dell'app macOS: passata;
- CTest: 68/68 passati, 0 falliti;
- eseguibile macOS presente e avviabile dal percorso del gate.
- smoke test offscreen `--grab`: passato.

Il nome del file temporaneo dello smoke test usa ora un template portabile
con `XXXXXX` in coda, compatibile con `mktemp` di macOS e Linux.

Il gate blocca quindi un artefatto locale quando build, test o versione
documentale falliscono, verifica anche l'avvio offscreen e la creazione di una
schermata smoke, invece di limitarsi alla compilazione dell'app. La CI
principale include inoltre un job separato per la suite Cloud. Il
comando è stato rieseguito il 2026-10-16, anche dopo la correzione di
`mktemp`: il processo del gate ha completato build, CTest e smoke test senza
nuovi errori osservati; il log finale conferma il completamento di
`udppipeline`. Il precedente `LastTestsFailed.log` non è stato aggiornato ed è
un residuo del 15 ottobre (`serialports`), già rieseguito con esito 1/1.

Il job Cloud separato è presente in `.github/workflows/ci.yml`; la dipendenza
`httpx` usata dal test client FastAPI è ora dichiarata in
`server/requirements.txt`. La suite Cloud è stata eseguita localmente con
Python 3.11: 136 test passati, 3 warning di dipendenze, 0 errori.

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
