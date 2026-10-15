# DXL-015 — QSL queue and reconciliation audit

QSL upload state is stored per QSO and service, including sent/received
status, remote identifier and last error. A confirmation is matched by the
normal callsign/date/time/band/mode rules; when more than one QSO is a
candidate, the result is `Ambiguous` and no QSO is modified.

The queue survives reopening the SQLite log. A failed attempt remains
retryable with its error, while a successful remote identifier prevents the
same service/QSO from being queued again. The controller exposes service,
account and active station profile in the service rows, and cancellation does
not mark a request as sent.

Verification on 2026-10-15:

- `tst_qsl`: passed 1/1, including ambiguous confirmation, persisted queue,
  retry state and idempotent sent state;
- `tst_qslcardcontroller`: passed 1/1, including cancellation and queue
  retention during a delayed Cloud response;
- `git diff --check`: passed.

The tests use local fakes and do not prove the availability or policy of the
external LoTW, QRZ, eQSL, Club Log or Wavelog services.
