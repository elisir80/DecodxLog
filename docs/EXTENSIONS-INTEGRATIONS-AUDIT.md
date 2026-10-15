# DXL-033 — Estensioni e integrazioni comunitarie con permessi

## Verifica — 16 ottobre 2026

Il programma dispone di integrazioni esplicite, non di un SDK generale per
plugin: API HTTP locale, UDP/DecoLink, cluster, callbook e servizi QSL. Le
integrazioni usano percorsi controllati dall'applicazione e non ricevono un
accesso diretto al database.

Sono stati rieseguiti i test dei confini più vicini alla voce:

```text
cmake --build build --parallel 4 --target tst_localapi tst_cluster tst_qsl tst_qsldownload
ctest --test-dir build --output-on-failure -R '^(localapi|cluster|qsl|qsldownload)$'
4/4 test superati (rieseguiti il 16 ottobre 2026)
```

Le prove coprono l'API locale autenticata, il traffico cluster, gli upload QSL
e gli importatori di conferme. Non è stato aggiunto alcun connettore esterno o
componente eseguito con privilegi nuovi.

## Gap rispetto all'estensione generale

Non esiste ancora un contratto versionato per pacchetti/adattatori fuori
processo, una tabella di permessi per estensione o un harness che dimostri che
un processo esterno terminato non interrompa il logger. Di conseguenza non è
possibile dichiarare soddisfatti i tre criteri completi della voce.

## Criteri per il lavoro successivo

Servono un manifest di estensione con versione e permessi dichiarati, API
mediata senza accesso al file SQLite, isolamento del processo e test di stop,
aggiornamento/disinstallazione con QSO invariati. L'invio TX dovrà essere
negato per default e richiedere un'autorizzazione esplicita dell'operatore.
