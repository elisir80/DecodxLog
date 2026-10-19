# DXL-030 — Assistente DX basato su priorità spiegabili

## Verifica — 16 ottobre 2026

Il ramo dispone già di tre sorgenti separate:

- gli spot del cluster, con stato worked/new e motivazioni come nuovo DXCC,
  banda, modo o slot;
- lo storico personale/worked-before;
- la previsione VOACAP.

Sono stati rieseguiti i test delle sorgenti:

```text
cmake --build build --parallel 4 --target tst_spots tst_cluster tst_voacap
ctest --test-dir build --output-on-failure -R '^(spots|cluster|voacap)$'
3/3 test superati (rieseguiti il 16 ottobre 2026).

Verifica corrente delle sorgenti separate dopo l'ultima modifica locale:

```text
cmake --build build --parallel 4 --target tst_spots tst_clustercontroller tst_propagation tst_voacap
ctest --test-dir build --output-on-failure -R '^(spots|clustercontroller|propagation|voacap)$'
4/4 test superati
```

Questa riesecuzione conferma le fonti di input. Il ranking composito e
l'adattatore verso la UI sono ora coperti dai test `dxassistant` e
`clustercontroller`; la verifica runtime con sorgenti esterne resta separata.
```

## Gap verificato prima dell'implementazione

L'assistente combina queste fonti in una classifica con punteggio, età del
dato, motivazione, evidenza e limiti. Il cluster continua a non eseguire TX
automatico e VOACAP resta una previsione esplicitamente distinta dai dati
osservati e storici.

## Implementazione incrementale — 16–19 ottobre 2026

È stato aggiunto `core::dxassistant::rank`, un nucleo deterministico che riceve
candidati già osservati, stato rispetto al log, previsione opzionale e orario
dello spot. Restituisce un punteggio ordinato, motivazioni e limitazioni esplicite.
Non esegue CAT, non modifica il database e non trasmette.

Il test `tst_dxassistant/ranksWithExplicitReasonsAndLimits` verifica un caso
nuovo DXCC/banda/referenza con propagazione buona contro un candidato vecchio e
senza previsione; `neverAddsTransmitOrLogSideEffects` verifica il contratto di
assenza di effetti collaterali.

È stato aggiunto anche l'adattatore `ClusterController::assistantRecommendations`:
legge gli spot visibili già arricchiti dal cluster e restituisce gli stessi
campi descrittivi al chiamante QML/API. Il test controller verifica un nuovo
DXCC con motivazione e limite di propagazione mancante.

```text
cmake -S . -B build -G Ninja -DDECODXLOG_BUILD_TESTS=ON
cmake --build build --parallel 4 --target tst_dxassistant
ctest --test-dir build -R '^dxassistant$' --output-on-failure
1/1 test superato

Verifica controller:

```text
cmake --build build --parallel 4 --target tst_clustercontroller
ctest --test-dir build -R '^clustercontroller$' --output-on-failure
1/1 test superato

La UI del DX Cluster ora espone il pulsante `DX priorities` e una finestra
dedicata che mostra nominativo, banda/modo, score, motivazioni e limitazioni.
La compilazione completa del target `decolog` ha ricreato anche il cache QML di
`ClusterPanel.qml` senza errori.

I pesi del ranking e la soglia di freschezza sono ora persistenti in
`cluster/assistantPreferences`, con clamp dei pesi a `0..100` e della soglia a
`1..240` minuti. Il test controller verifica lettura/scrittura e i limiti.
Il popup espone inoltre sei controlli grafici numerici per DXCC, banda, modo,
riferimento, propagazione e freschezza; ogni modifica passa dalla stessa API
validata e viene salvata nelle preferenze.

Il cluster usa ora un adapter di sola lettura verso `SolarController`: per ogni
spot con percorso disponibile estrae la qualità della banda nell'ora corrente
e l'origine della previsione (`voacap` o `model`). Il risultato viene esposto
insieme a motivazioni e limitazioni, senza CAT, modifica del log o TX.
```
```

## Criteri per l'implementazione successiva

Prima di aggiungere il ranking servono una specifica versionata per:

1. obiettivi scelti dall'operatore e relativi pesi;
2. freschezza degli spot e della previsione;
3. disponibilità di banda, modo, antenna e rotore;
4. formato della spiegazione e dei limiti quando una fonte manca;
5. divieto esplicito di inventare nominativi/QSO e di trasmettere.

Il nucleo ora espone anche l'origine dell'evidenza (`observed`, `historical`,
`forecast/inferred`) e può applicare vincoli dichiarativi su radio e antenna,
oltre a banda, modo e account diploma. La UI permette di configurare gli
elenchi hardware; un elenco vuoto significa "qualsiasi". La conferma di uno
spot viene derivata dal flag `StatusUnconfirmed`, senza inventare una QSL.

I test verificano i tre stati di evidenza, il filtro radio/antenna, il caso
worked-but-unconfirmed e la persistenza delle preferenze. Restano verifiche
esterne: il comportamento con un cluster reale e una previsione VOACAP
completata, perché una richiesta può inizialmente essere pending e gli spot
privi di locatore restano senza previsione.
