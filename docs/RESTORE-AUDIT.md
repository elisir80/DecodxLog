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

Il manifest dei bundle nuovi contiene anche le impronte SHA-256 del database e
di ogni risorsa. `aCorruptedStationBundleDoesNotTouchTheTargetLog` altera una
risorsa dopo la creazione, verifica il rifiuto per integrità e dimostra che il
log di destinazione e le risorse installate restano invariati. I bundle storici
senza la sezione `sha256` restano leggibili per compatibilità.

Verifica eseguita e ripetuta il 16 ottobre 2026:

```text
cmake --build build --parallel 4 --target tst_logbackup
ctest --test-dir build -R '^logbackup$' --output-on-failure
```

Risultato: 1 test passato, 0 falliti (`logbackup`: 1/1), inclusi i casi di
bundle incompleto e bundle alterato.

Verifica aggiuntiva su volume separato, 16 ottobre 2026: l'intera suite
`tst_logbackup` è stata eseguita con `TMPDIR` dentro un'immagine APFS sparse
montata tramite `hdiutil`. Risultato: 14 test passati, 0 falliti, inclusi i
casi di creazione atomica, ripristino di database e risorse e rifiuto di bundle
alterati.

Limite residuo: il test è automatizzato in directory temporanee sullo stesso
host o su un'immagine montata, e non sostituisce ancora una prova su una
macchina pulita né una simulazione di interruzione del processo durante la
copia fisica di un file molto grande.
