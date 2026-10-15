# Logbook migration audit

The import path accepts ADIF plus the documented CSV/TSV and N1MM conversions.
Before confirmation, the import dialog reports the source kind, records read,
estimated new records, duplicates and rejected records, incomplete UTC, QSL
status-bearing records, and preserved unmapped fields. The estimate is made in
a private in-memory copy of the current log; the real database is not touched.

The committed import report also records inserted IDs, duplicates, invalid
records, incomplete UTC, QSL-bearing records and unmapped fields. Imports are
performed in batches and the controller retains the inserted IDs so the last
import can be undone: the QSO rows are moved to recycle/history rather than
being hard-deleted.

Verified locally:

- ADIF round-trip preserves mapped and extra fields.
- QSL states and UTC/mode fields survive import/export.
- Duplicate detection works across batches.
- `ctest -R 'migration|database|qmlpopups'` passes.

The before/after estimate is intentionally advisory: the final report remains
authoritative because another local QSO can arrive between preview and import.

Verification on 2026-10-15: the focused migration and import-preview tests
passed (`migration` 1/1 and `qmlpopups` 1/1). The tests cover Italian and
English CSV, N1MM SQLite, ADIF round-trip, UTC/mode normalization, QSL states,
duplicate handling, preview counts and undo through the inserted-ID path.
Real exports from Log4OM, DXKeeper or other third-party versions were not
available in this checkout, so their vendor-specific presets remain unclaimed.
