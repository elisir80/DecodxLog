# DecoDXLog 1.17.24

## Italiano

Aggiornamenti dalla `1.17.23` alla `1.17.24`.

- Rafforzata l'integrità dei bundle di backup della stazione: il manifest ora contiene le impronte SHA-256 del database e delle risorse incluse.
- Il ripristino verifica le impronte prima di sostituire il log o pubblicare le risorse; bundle mancanti, alterati o corrotti vengono rifiutati senza toccare il log di destinazione.
- Aggiunto un test multipiattaforma per la corruzione di una risorsa nel bundle.
- Mantenuti i miglioramenti CAT e seriali introdotti nella 1.17.23.

## English

Updates from `1.17.23` to `1.17.24`.

- Strengthened station-backup bundle integrity: the manifest now contains SHA-256 fingerprints for the database and included resources.
- Restore verifies fingerprints before replacing the log or publishing resources; missing, modified, or corrupted bundles are rejected without touching the destination log.
- Added a cross-platform test covering corruption of a bundle resource.
- Retained the CAT and serial improvements introduced in 1.17.23.
