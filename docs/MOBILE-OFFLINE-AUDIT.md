# DXL-023 — Client portatile con registrazione realmente offline

## Stato verificato — 16 ottobre 2026

La voce richiede un client mobile/portatile capace di registrare QSO senza
rete, conservarli dopo il riavvio e sincronizzarli senza duplicazioni o
sovrascritture implicite.

Nel ramo esaminato non risultano target Android/iOS, codice Qt for Android/iOS
o un client portatile separato. Il programma disponibile è il client desktop
offline-first; il Cloud e l'interfaccia browser non dimostrano da soli la
presenza di una coda locale mobile.

## Evidenze eseguite

- `tst_database`: superato. Verifica il salvataggio locale senza attendere un
  servizio esterno irraggiungibile e la persistenza/riapertura del database
  SQLite tramite un test di chiusura/riapertura e il percorso di backup. Il
  QSO, lo stato QSL e la coda dirty restano presenti dopo il riavvio del
  database.
- `tst_cloud`: superato. Verifica la gestione della coda dirty e dei conflitti
  provenienti da un client offline, inclusa la protezione dalla resurrezione di
  un QSO cancellato.
- `tst_migration`: superato. Verifica i percorsi di importazione dati rilevanti
  per un futuro client/import-export.

Comando e risultato osservato:

```text
cmake --build build --parallel 4 --target tst_database tst_cloud tst_migration
ctest --test-dir build --output-on-failure -R '^(database|cloud|migration)$'
3/3 test superati
```

La stessa suite è stata rieseguita il 16 ottobre 2026: `database`, `cloud` e
`migration` hanno superato tutti i test (3/3).

La prova focalizzata ripetuta dopo l'aggiunta del caso di riapertura è:

```text
cmake --build build --parallel 4 --target tst_database
ctest --test-dir build --output-on-failure -R '^database$'
1/1 test superato
```

## Criteri ancora non soddisfatti

1. Target e packaging mobile reale.
2. Inserimento di QSO in una sessione senza rete su dispositivo mobile.
3. Conservazione della coda dopo chiusura e riavvio del client mobile.
4. Sync successivo end-to-end con deduplicazione e politica esplicita per i
   conflitti.
5. Test su Android/iOS e connettività alternata.

## Decisione

DXL-023 resta aperto e non viene implementato parzialmente in questo passaggio:
aggiungere solo una UI mobile o una coda fittizia non soddisferebbe i criteri
di accettazione. La copertura offline desktop e la riconciliazione Cloud
esistenti sono prerequisiti utili, ma non sostituiscono il client portatile.
