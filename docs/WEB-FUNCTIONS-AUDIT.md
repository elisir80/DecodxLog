# DXL-025 — Funzioni web richieste

## Verifica — 16 ottobre 2026

Il ramo contiene già un Cloud/web log con export ADIF, selezione del profilo
stazione, pagine QSL e dati di sincronizzazione. Le autorizzazioni distinguono
il proprietario, l'operatore e l'osservatore: il viewer può leggere ma non
pubblicare modifiche.

Sono stati rieseguiti i test che coprono il contratto web e i documenti:

```text
pytest -q server/tests/test_web.py server/tests/test_team.py \
  server/tests/test_docs.py server/tests/test_qslmail.py
54 passed (rieseguiti il 16 ottobre 2026)
```

Le prove verificano in particolare export ADIF, export filtrato per profilo,
accesso autenticato, ruoli operator/viewer, documenti di profilo/impostazioni,
QSL e comportamento delle revisioni. La parità dei risultati award desktop/web
è coperta separatamente dalla fixture condivisa di DXL-028.

## Limiti

- Le etichette QSL PDF e i file contest restano funzioni desktop; il web espone
  coda e stato, non una copia indiscriminata dell'interfaccia desktop.
- Non è stata aggiunta una nuova operazione web senza un requisito concreto e
  autorizzato.
- Non sono stati eseguiti importatori proprietari o un ambiente Cloud remoto:
  i test sono locali e non dimostrano disponibilità operativa di un deployment.

La voce è coperta per le funzioni web attualmente definite, ma resta aperta
per eventuali documenti QSL/export web aggiuntivi che dovranno essere
specificati e verificati con il relativo servizio.
