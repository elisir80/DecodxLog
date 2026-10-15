# Ambito DXCC e propagazione — audit DXL-004

## Comportamento verificato

- I QSO con `PROP_MODE=SAT` o con `SAT_NAME` valorizzato restano nel log, ma
  non entrano nel credito DXCC/WAS terrestre.
- Le entità DXCC invalidate o cancellate vengono escluse quando il catalogo
  delle regole fornisce il relativo predicato.
- Il calcolo conserva il dettaglio dell'esclusione nel requisito del risultato,
  invece di eliminare silenziosamente il QSO.
- I filtri per profilo, banda, modo, tag e periodo sono applicati al calcolo.

I casi di satellite, profilo, banda, modo, tag e di entità storicamente
invalida sono coperti da `tests/tst_awards.cpp`; il 2026-10-15 il test
`awards` è passato 1/1.

## Lacuna ancora aperta

Il profilo “tutti i profili” non determina ancora automaticamente un account
DXCC ufficiale in base all'entità DXCC della stazione emittente. I profili
separati possono essere scelti esplicitamente, ma non esiste ancora un modello
versionato di luogo operativo, periodo di validità e regola per impedire la
somma automatica di QSO provenienti da entità diverse. Non viene quindi
presentata come risolta la parte geografica dell'acceptance criterion.

Riferimenti ufficiali da mantenere nella futura implementazione:

- [ARRL DXCC rules](https://www.arrl.org/dxcc-rules)
- [ARRL DXCC award information](https://www.arrl.org/dxcc-award-information)

Prima di implementare la separazione automatica serviranno fixture per profili
con entità/periodi diversi, gestione dei cambi di nominativo e una regola
versionata per lo storico; nessuna modifica speculativa è stata introdotta in
questa voce.
