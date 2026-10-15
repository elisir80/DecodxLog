# SO2R e rete contest — audit DXL-021

## Stato verificato

- Il cambio di fuoco limita le operazioni di sintonia, modo, CW e PTT al
  `RigLink` selezionato; quando il fuoco è sulla radio 2, il controller usa il
  collegamento alternativo invece di quello della radio 1.
- La scatola SO2R riceve comandi OTRSP coerenti con il fuoco (`TX1`/`RX1` o
  `TX2`/`RX2`), con l’estensione stereo solo sul canale selezionato.
- Lo stop di una macro è già coperto dal test del rig controller e viene
  inviato al percorso radio che ha ricevuto la macro.
- `SharedSerials` assegna il progressivo dal distributore quando la rete è
  disponibile, conserva la prenotazione sul client e segnala le collisioni
  quando due client lavorano offline. I casi sono coperti dai test contest.

## Test eseguiti

```text
cmake --build build --parallel 4 --target tst_so2r tst_contestrules tst_rigcontroller decolog
ctest --test-dir build -R '^(so2r|contestrules|rigcontroller)$' --output-on-failure
```

Risultato: 3 test passati, 0 falliti. La build dell’applicazione è completata.

## Limiti e collaudo residuo

Questi test simulano il collegamento radio 2 con un server locale e la rete dei
progressivi in memoria. Non dimostrano il comportamento di una coppia di radio,
di una scatola OTRSP o di una LAN reale durante disconnessioni, ritardi e
riavvii. Serve ancora un collaudo hardware/rete per chiudere completamente il
criterio operativo.

Il fuoco software non è un interlock RF. Per impedire fisicamente due
trasmettitori contemporanei resta necessario l’interlock hardware previsto
dall’impianto della stazione; DecoDXLog non deve essere presentato come suo
sostituto.

## Verifica ripetuta — 15 ottobre 2026

La verifica focalizzata è stata rieseguita sul checkout macOS arm64 corrente:

```text
cmake --build build --parallel 4 --target tst_so2r tst_contestrules tst_rigcontroller decolog
ctest --test-dir build -R '^(so2r|contestrules|rigcontroller)$' --output-on-failure
3/3 test superati
```

Questo conferma i percorsi simulati di fuoco sulla radio 2, stop della macro e
regole contest. Non sostituisce una prova con due radio, scatola OTRSP o LAN
reale durante guasti, ritardi e riavvii.
