# Cloud settings scopes

The settings sync uses policy version `1` (`cloudsettings::kScopeVersion`).
The classification is evaluated before a value is collected or applied:

- `Portable`: themes, macros, filters and other settings that describe the
  operator's preferred workflow.
- `Station`: station-profile identity and station-profile records, resolved by
  UUID rather than the local database row number.
- `Device`: serial/audio/network ports, local service addresses, paths,
  credentials metadata, UDP identity and sync bookkeeping. These values never
  overwrite the receiving computer.

The device policy is intentionally conservative for radio and filesystem
settings. A new scope version is required if a key changes classification.
The contract is covered by `tst_cloudsettings`, including a dry-run and a
cross-machine apply check. It was re-run on 2026-10-15 with 1/1 test passed.
No database schema or ADIF format is changed.
