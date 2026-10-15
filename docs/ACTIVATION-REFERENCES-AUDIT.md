# DXL-024 — Workflow avanzato per attivazioni e referenze multiple

## Verifica — 15 ottobre 2026

Il modello di sessione conserva una stringa di referenza e la applica ai campi
ADIF dell'attivatore (`MY_SIG_INFO`, `MY_POTA_REF`, `MY_WWFF_REF`,
`MY_SOTA_REF` o `MY_IOTA`). Le referenze multiple non vengono ridotte a un
numero fisso di QSO e restano associate a ogni QSO della sessione.

È stata aggiunta una validazione sintattica comune: gli elementi separati da
virgola vengono ripuliti, normalizzati in maiuscolo e controllati contro
elementi vuoti o duplicati. La validazione non decide se il formato appartenga
a un programma specifico.

È stato aggiunto un test di round-trip che crea due QSO con la stessa sessione
POTA e due referenze, esporta l'ADIF e lo rilegge con il parser interno. Per
entrambi i record restano intatti le due referenze e il locatore.

```text
cmake --build build --parallel 4 --target tst_activation
ctest --test-dir build --output-on-failure -R '^activation$'
1/1 test superato
```

## Limiti

- Non è stata inventata una validazione regolamentare per park-to-park,
  cambi di sito o referenze concomitanti: servono regolamenti specifici e
  versionati per ogni programma.
- Non è stata eseguita una prova con importatori proprietari o servizi remoti;
  il test dimostra la conservazione ADIF interna, non la compatibilità di ogni
  programma esterno.
- La voce resta parzialmente aperta finché non saranno definiti i programmi
  supportati e i relativi casi di import/export end-to-end.
