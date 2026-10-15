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
invalida sono coperti da `tests/tst_awards.cpp`; il 2026-10-16 il test
`awards` è passato 1/1. La fixture
`officialCountsDoNotMixExplicitStationAccounts` verifica inoltre che due
profili di stazione espliciti, associati a entità diverse, producano conteggi
separati e non vengano sommati dal filtro ufficiale.

## Lacuna ancora aperta

Per una nuova installazione il controller seleziona il profilo di stazione
attivo; un profilo scelto esplicitamente filtra i QSO associati a quel profilo.
Resta però una lacuna: il calcolatore non determina automaticamente un account
DXCC ufficiale in base all'entità DXCC del luogo operativo del profilo. Non
esiste ancora un modello versionato di luogo operativo, periodo di validità e
regola per impedire la somma automatica di QSO provenienti da entità diverse.
La parte geografica dell'acceptance criterion resta quindi aperta.

Riferimenti ufficiali da mantenere nella futura implementazione:

- [ARRL DXCC rules](https://www.arrl.org/dxcc-rules)
- [ARRL DXCC award information](https://www.arrl.org/dxcc-award-information)

Prima di implementare la separazione automatica serviranno gestione dei cambi
di nominativo e una regola versionata per lo storico; nessuna modifica
speculativa è stata introdotta in questa voce.
