# Enrichment provenance audit

Callbook enrichment records the field, previous value, resulting value,
source, acquisition time, kind (`inferred`) and validity interval. The QSO
detail view exposes these entries and permits undoing an enrichment; undo
restores the prior value through the normal revision/history path.

Inferred geographic/award fields (`DXCC`, `CQZ`, `STATE`, `GRIDSQUARE`) are
excluded from award calculations until they are supplied by the QSO or an
explicitly declared/received confirmation. The values remain visible in the
QSO and can be reviewed or undone, but a current callbook lookup cannot
silently change historical award eligibility.

Verified locally:

- enrichment provenance and undo pass in `database` tests;
- callbook field-filling tests preserve operator-entered values;
- award calculation ignores an inferred DXCC value;
- `awards` passes after the provenance guard.

Verification repeated on 2026-10-15: `database`, `callbook` and `awards`
passed 3/3. The tests cover provenance/undo, protection of operator-entered
fields, and exclusion of inferred geographic values from award eligibility.

This policy deliberately does not claim that a callbook record proves the
station's historical location. An operator can still correct or confirm the
field explicitly through the normal QSO edit/history workflow.
