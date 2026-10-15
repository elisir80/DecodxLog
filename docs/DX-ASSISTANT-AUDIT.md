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
3/3 test superati (rieseguiti il 16 ottobre 2026)
```

## Gap verificato

Non esiste ancora un assistente unico che combini queste fonti in una classifica
con punteggio, età del dato, motivazione e limiti. L'attuale cluster mostra
stati e filtri, ma non dichiara una priorità composita; VOACAP resta una
previsione separata. Non è stato introdotto alcun TX automatico.

## Criteri per l'implementazione successiva

Prima di aggiungere il ranking servono una specifica versionata per:

1. obiettivi scelti dall'operatore e relativi pesi;
2. freschezza degli spot e della previsione;
3. disponibilità di banda, modo, antenna e rotore;
4. formato della spiegazione e dei limiti quando una fonte manca;
5. divieto esplicito di inventare nominativi/QSO e di trasmettere.

La voce resta aperta: le sorgenti di base sono verificate, ma l'assistente
spiegabile richiesto non è ancora presente.
