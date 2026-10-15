# Ciclo domanda–verifica–credito — audit DXL-011

## Comportamento verificato

Il motore distingue l'eleggibilità derivata dalle conferme del QSO dalla
domanda di diploma (`AwardClaim`). Una conferma LoTW/cartacea entra nel
calcolo dell'award secondo il filtro, ma non crea né porta automaticamente una
domanda allo stato `Credited`.

Le domande conservano identificativo del diploma, versione della regola, QSO
presentati, esito, evidenza e timestamp. Le transizioni consentite sono:

```text
Draft -> Submitted -> Verified -> Credited
                     \-> Rejected
```

La UI degli award consente di creare la domanda e di avanzarla esplicitamente,
registrando nota ed evidenza. Il caricamento e il salvataggio avvengono nel log
locale; non è stata inventata alcuna API automatica di un ente esterno.

## Verifica locale

`tests/tst_awards.cpp` (`awardClaimSeparatesConfirmationFromSubmissionAndCredit`)
verifica che:

- il credito diretto da `Draft` sia rifiutato;
- `Submitted` richieda una verifica prima del credito;
- una domanda rifiutata non possa diventare accreditata;
- QSO, versione, esito ed evidenza sopravvivano alla serializzazione e al
  salvataggio nel database.

Il test `awards` è passato 1/1 il 2026-10-16.

## Limiti

La verifica dell'esito resta operativa/manuale: l'applicazione conserva la
domanda e la risposta dell'ente, ma non può attestare da sola che un credito
esterno sia stato realmente concesso.
