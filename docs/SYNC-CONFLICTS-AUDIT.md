# Conflitti e convergenza del sync — audit DXL-029

## Comportamento verificato

- `LogDatabase::applyRemote()` accetta solo revisioni non più vecchie di quella
  locale e conserva le revisioni sostituite in `qso_history`.
- Una modifica locale ancora `dirty` non viene sovrascritta da un remoto della
  stessa o di una revisione inferiore: torna nella coda di invio.
- Una cancellazione remota è una cancellazione morbida; un contenuto vecchio
  arrivato da un client offline non può resuscitare il QSO.
- L'ordine è determinato dalla revisione del record, non dall'orologio del
  dispositivo; quindi un clock skew non cambia la decisione di convergenza.

## Test

`tests/tst_cloud.cpp` copre modifica remota più nuova, storico, modifica
locale non sovrascritta, cancellazione remota e il caso aggiunto
`anOlderOfflineClientCannotResurrectADeletedQso`.

```text
cmake --build build --parallel 4 --target tst_cloud
ctest --test-dir build -R '^cloud$' --output-on-failure
1/1 passed
```

## Limite

La prova è deterministica e locale: non sostituisce un collaudo con due
processi, servizio Cloud e rete interrotta/riattivata. Il protocollo e lo
storico sono verificati, ma il comportamento trasporto/retry deve essere
validato separatamente in un ambiente di integrazione.

## Verifica ripetuta — 15 ottobre 2026

È stato aggiunto `clockSkewDoesNotOverrideRevisionOrder` a
`tests/tst_cloud.cpp`: una revisione più nuova vince con timestamp remoto
vecchio, mentre una revisione vecchia con timestamp futuro viene scartata.

```text
cmake --build build --parallel 4 --target tst_cloud
ctest --test-dir build -R '^cloud$' --output-on-failure
1/1 passed
```

Il test conferma che l’ordine decisionale non dipende dall’orologio locale.
Restano da verificare con due processi e un servizio reale le interruzioni del
trasporto, i retry e la riconnessione.
