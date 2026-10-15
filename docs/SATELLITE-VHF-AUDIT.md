# Satelliti e VHF — audit DXL-022

## Esito dell'ispezione

Il ramo corrente conserva il campo ADIF `SAT_NAME` e può mostrare dati di
azimut/rotore, ma non contiene ancora un modulo di tracking satellitare. Non
sono presenti un catalogo di elementi Kepleriani/TLE, il calcolo SGP4 dei
passaggi, la gestione AOS/LOS, il calcolo Doppler separato per uplink e
downlink, né un protocollo di integrazione con un tracker esterno.

Di conseguenza non è corretto presentare la funzione esistente come supporto
satellitare completo. Il controllo del rotore e il CAT generico restano
funzioni indipendenti e non avviano trasmissioni automaticamente.

## Verifica esterna dei requisiti

La documentazione ufficiale di Ham Radio Deluxe descrive come requisiti gli
elementi Kepleriani, i passaggi e le frequenze uplink/downlink corrette per il
Doppler; descrive inoltre il caso a due radio. Questi sono requisiti di
prodotto, non assunzioni da ricavare dai soli campi ADIF.

Riferimenti consultati:

- Ham Radio Deluxe, [Satellite Tracking introduction](https://support.hamradiodeluxe.com/support/solutions/articles/51000052558-introduction)
- Ham Radio Deluxe, [Satellites and Keplerian elements](https://support.hamradiodeluxe.com/support/solutions/articles/51000052573)
- Ham Radio Deluxe, [radio-specific satellite frequency handling](https://support.hamradiodeluxe.com/support/solutions/articles/51000052576-information-on-specific-radios)

## Criteri per una futura implementazione

Prima di aggiungere UI o comandi CAT servono:

1. un contratto documentato per il tracker supportato e fixture di protocollo;
2. test deterministici per passaggi, Doppler, uplink/downlink e perdita della
   connessione;
3. un modello ADIF che conservi correttamente satellite, propagazione e
   frequenze senza confonderle con la frequenza corrente della radio;
4. un consenso esplicito dell'operatore per ogni modifica CAT/PTT, con test che
   dimostrino l'assenza di TX automatico come effetto collaterale.

Nessuna modifica applicativa è stata introdotta per questa voce: implementarla
senza aver scelto e verificato un protocollo tracker produrrebbe una funzione
solo apparentemente completa.

## Verifica ripetuta — 16 ottobre 2026

Il percorso dati già disponibile è stato rieseguito:

```text
cmake --build build --parallel 4 --target tst_adif tst_database tst_awards
ctest --test-dir build --output-on-failure -R '^(adif|database|awards)$'
3/3 test superati
```

I test confermano la conservazione di `FREQ`, `FREQ_RX`, `PROP_MODE`,
`SAT_NAME` e `SAT_MODE` nel ciclo ADIF/database e la compatibilità dei QSO
satellitari con i conteggi correnti. Non dimostrano tracking, Doppler, AOS/LOS
o compatibilità con tracker esterni: DXL-022 resta non implementata come
modulo specialistico.
