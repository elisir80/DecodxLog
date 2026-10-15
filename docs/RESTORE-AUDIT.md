# Ripristino del patrimonio della stazione — audit DXL-027

Il backup della stazione viene creato come bundle atomico: copia SQLite
consistente, impostazioni portabili, risorse esterne e manifest. I segreti del
portachiavi e l'identità della macchina non vengono copiati nel bundle.

Il test `tst_logbackup/restoringAStationBundleRestoresDatabaseAndResources`
verifica ora anche:

- profilo di stazione e relativo nominativo;
- QSO con stato QSL;
- allegato QSL ricevuto, con contenuto e collegamento al QSO;
- revisione e storico del QSO;
- risorsa esterna del voice keyer;
- eliminazione della directory temporanea dopo il ripristino.

Il test `anIncompleteStationBundleDoesNotTouchTheTargetLog` verifica che una
risorsa mancante lasci il log di destinazione invariato. La creazione del bundle
usa una directory `.partial` e il rename finale, così un'interruzione non rende
visibile una copia incompleta come backup valido.

Verifica eseguita e ripetuta il 16 ottobre 2026:

```text
cmake --build build --parallel 4 --target tst_logbackup
ctest --test-dir build -R '^logbackup$' --output-on-failure
```

Risultato: 1 test passato, 0 falliti (`logbackup`: 1/1).

Limite residuo: il test è automatizzato in directory temporanee sullo stesso
host. Non sostituisce ancora una prova su una macchina pulita o su un volume
removibile, né una simulazione di interruzione del processo durante la copia
fisica di un file molto grande.
