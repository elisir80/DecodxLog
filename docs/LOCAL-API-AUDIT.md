# DXL-018 — audit API locale

## Verifica locale

Il test di integrazione `udppipeline` verifica il contratto della API locale
insieme all'applicazione reale:

- la stessa `Idempotency-Key` e lo stesso corpo restituiscono la stessa risposta
  senza creare un secondo QSO;
- riusare la chiave con un corpo diverso restituisce `409`;
- un contatto distinto resta distinto;
- una richiesta senza token restituisce `401`.

Verifica eseguita il 2026-10-16:

```text
cmake --build build --parallel 4 --target tst_udppipeline
ctest --test-dir build --output-on-failure -R '^udppipeline$'
1/1 Test #69: udppipeline ... Passed
```

La chiave di idempotenza è in memoria e decade al riavvio; il contratto non
promette deduplicazione oltre la durata del processo. Il token non viene
incluso nel corpo delle risposte, mentre la diagnostica usa la redazione delle
credenziali verificata da `tst_diagnostics`.
