# WAC e Antartide — audit DXL-001

La regola ufficiale ARRL/IARU richiede sei continenti: North America, South
America, Oceania, Asia, Europe e Africa. L'Antartide può restare visibile come
informazione geografica, ma non deve aumentare il credito WAC ufficiale.

Il calcolatore separa ora la visualizzazione dal credito: l'item `AN` viene
conservato con `countsForAward = false`, mentre `worked(true)`, `confirmed(true)`
e i totali per banda escludono l'Antartide. L'Oceania (`OC`) completa invece il
sesto continente ufficiale.

Test eseguito:

```text
cmake --build build --parallel 4 --target tst_awards
ctest --test-dir build -R '^awards$' --output-on-failure
```

Risultato: test passato, 0 fallimenti. La fixture verifica che l'Antartide
resti nell'elenco visualizzato e non nei conteggi ufficiali, che Oceania porti
il totale ufficiale a sei e che i totali per banda rispettino lo stesso filtro.
La fixture Cloud condivisa è stata rieseguita con `pytest -q
server/tests/test_analytics.py`: 20 test passati.

Fonte della regola: [ARRL WAC](https://www.arrl.org/wac), che elenca i sei
continenti validi per il diploma.

Limite: questo checkout non contiene un calcolatore Cloud indipendente da
confrontare in esecuzione; la parità desktop/Cloud resta da verificare contro
un servizio remoto o una fixture condivisa del backend.
