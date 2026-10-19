# DecoDXLog 1.17.27

## Italiano

Aggiornamenti dalla `1.17.26` alla `1.17.27`.

- Consolidata la separazione tra operatore, luogo operativo, profilo tecnico ed entità DXCC, con migrazioni SQLite, controlli di compatibilità e segnalazione dei conflitti tra account diploma.
- Estesa la gestione degli account diploma: raggruppamento sicuro dei profili compatibili, rifiuto delle aggregazioni ambigue e collegamento dell’account scelto agli obiettivi del DX Assistant.
- Completato il percorso del DX Assistant con obiettivi worked-but-unconfirmed, account diploma, fattibilità rispetto a radio/antenna/banda/modo e distinzione tra dati osservati, storico e previsioni.
- Aggiunti pacchetti dichiarativi e verificabili per le regole dei diplomi, inclusi VUCC e CQ DX Marathon, con validazione, fingerprint e risultati esportabili.
- Aggiunta la riconciliazione dei claims esterni per QSO, categoria e account, con importazione controllata e report degli elementi non riconciliati.
- Aggiunta una matrice di migrazione con fixture ADIF/CSV/SQLite per Log4OM, DXKeeper e N1MM, oltre alla matrice di collaudo operativo e al report diagnostico strutturato.
- Rafforzati i test automatici di database, rete, QML, diplomi, migrazioni, performance e diagnostica; il release gate verifica anche avvio, screenshot e chiusura pulita.
- I pacchetti ufficiali vengono prodotti dai workflow manuali multipiattaforma con OpenSSL richiesto, smoke test del pacchetto e manifest firmati per l’aggiornamento automatico.

## English

Updates from `1.17.26` to `1.17.27`.

- Consolidated the separation of operator, operating location, technical profile, and DXCC entity, including SQLite migrations, compatibility checks, and diploma-account conflict reporting.
- Extended diploma-account handling with safe grouping of compatible profiles, rejection of ambiguous aggregations, and explicit binding of the selected account to DX Assistant objectives.
- Completed the DX Assistant path with worked-but-unconfirmed objectives, diploma accounts, radio/antenna/band/mode feasibility, and explicit observed, historical, and forecast evidence labels.
- Added declarative, verifiable award-rule packages, including VUCC and CQ DX Marathon, with validation, fingerprints, and exportable results.
- Added controlled reconciliation of external claims by QSO, category, and account, including a report for unreconciled items.
- Added a migration matrix with ADIF/CSV/SQLite fixtures for Log4OM, DXKeeper, and N1MM, together with the operational-collation matrix and structured diagnostics report.
- Strengthened automated tests for database, networking, QML, awards, migrations, performance, and diagnostics; the release gate also checks startup, screenshots, and clean shutdown.
- Official packages are produced by manual cross-platform workflows with required OpenSSL, package smoke tests, and signed update manifests.

The release assets are published only after each requested runner completes successfully and the exact non-empty files are verified.
