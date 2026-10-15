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
- CTest: 67/67 passati, 0 falliti;
- eseguibile macOS presente e avviabile dal percorso del gate.

Il gate blocca quindi un artefatto locale quando build, test o versione
documentale falliscono, invece di limitarsi alla compilazione dell'app. Il
comando è stato rieseguito il 2026-10-15: 67/67 test passati e gate concluso.

## Limiti residui

La verifica corrente è stata eseguita su macOS arm64. Non prova l'avvio su una
macchina pulita, l'accesso al portachiavi in un ambiente nuovo, né i runner
Windows, Linux x86_64, Linux aarch64 e macOS Intel. Questi devono restare
controlli obbligatori nei workflow CI prima della promozione di un pacchetto.
