# DXL-019 — diagnostica guidata delle integrazioni

## Verifica locale

Il percorso «Integration diagnostics» espone le fasi ricezione, parsing,
deduplicazione, salvataggio e QSL con stato leggibile dall'operatore. Il report
di supporto viene redatto localmente e contiene solo valori passati attraverso
la redazione delle credenziali. Oltre al report testuale è disponibile un
report JSON strutturato con schema versionato, timestamp, passi osservati,
ambito di verifica, limiti e metriche di latenza UI.

La UI dichiara esplicitamente che i controlli restano sul computer locale e
non esegue invii di prova. Il file viene scritto soltanto dopo il pulsante
un pulsante di salvataggio esplicito e la conferma del selettore file. Il JSON
viene salvato separatamente dal report testuale.

Verifica eseguita il 2026-10-16:

```text
cmake --build build --parallel 4 --target tst_qmlpopups
ctest --test-dir build --output-on-failure -R '^qmlpopups$'
1/1 Test #58: qmlpopups ... Passed
```

`tests/tst_diagnostics.cpp` verifica inoltre la redazione di token, API key e
password. Il test UDP end-to-end verifica il percorso reale di ricezione,
deduplicazione e salvataggio.

La suite QML verifica entrambi i percorsi di esportazione e il target
`diagnostics` verifica che i controlli hardware sensibili, incluso il PTT,
restino `not-run` senza consenso esplicito.

Limite: la diagnostica non simula un servizio QSL pubblico e non certifica la
raggiungibilità di una radio reale; mostra lo stato locale e lascia l'eventuale
azione esterna all'operatore.
