# N126 operational collaudo matrix

The machine-readable source is `tests/fixtures/operational_matrix.json` and
is validated by `tst_diagnostics`. It deliberately separates evidence classes:

- `automatic`: local deterministic checks; currently passed for diagnostics;
- `ui`: operator session required on each supported desktop platform;
- `hardware`: named radio, CAT/audio path, antenna and explicit PTT approval;
- `external-data`: anonymized exports or external service results.

Only the automatic row is marked `passed` in the fixture. UI, hardware and
external-data rows remain `pending` until their required evidence exists. This
prevents a green local test suite from being presented as a real-radio or
cross-platform operational certification.
