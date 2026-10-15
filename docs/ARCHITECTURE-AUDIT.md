# Architecture and service-boundary audit

This is the current evidence for DXL-032. It is deliberately incremental: it
does not claim that the whole application has already been refactored into
independent services.

## Verified

- `decolog_core` contains the local database and award domain used by
  `tst_database` and `tst_awards`.
- `tst_awards` uses `QTEST_GUILESS_MAIN`, so award calculations are exercised
  without a QML window, audio device or physical radio.
- `tst_database::localWriteDoesNotWaitForUnavailableService` starts an
  unavailable external-service request and verifies that a local QSO is still
  inserted within the local-operation budget.
- The existing large-log benchmark records import/recompute latency and RSS;
  see `docs/PERFORMANCE-BENCHMARKS.md`.

## Limits

- The QSL rendering sources are now compiled with the application target and
  with their focused QSL tests, rather than being part of the reusable core
  source list. `decolog_core` still links QtGui for the WSJT-X protocol's
  QColor serialization, so it is not yet a GUI-free target.
- The timeout test proves the database boundary, not every UI/controller path.
- No cross-platform runtime test has yet been executed on Windows or Linux,
  and no full service-failure fault-injection matrix exists yet.
- No data-format migration was introduced by this incremental change.

## Verifica ripetuta — 15 ottobre 2026

```text
ctest --test-dir build --output-on-failure -R '^(architecture|database|awards)$'
3/3 test superati
```

La prova conferma il salvataggio locale senza attesa del servizio esterno,
il dominio diplomi senza GUI e i controlli architetturali correnti. Restano
non verificati i runtime Windows/Linux e una matrice completa di fault
injection dei servizi.
