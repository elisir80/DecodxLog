# DecoDXLog extension goal — verification matrix

This matrix is intentionally conservative: `implemented` means the local
code and focused tests cover the contract; `pending` means evidence requires
hardware, another machine, an external service or a real vendor export.

| Requirement | Local implementation/evidence | Status |
|---|---|---|
| Separate operator, location and DXCC entities | Schema migration 11, normalized tables, `StationProfileModel`, `tst_database`, `tst_stationprofile` | implemented |
| Safe award-account aggregation | `awardAccountGroups()`, conflict reasons, compatible/non-compatible fixtures | implemented |
| DX Assistant evidence/ranking | `DxAssistant`, `ClusterController`, persisted preferences, UI and `tst_dxassistant`/`tst_clustercontroller` | implemented |
| Worked-but-unconfirmed, account and feasibility filters | Core ranking plus band/mode/radio/antenna/account UI controls | implemented |
| Declarative award packages | `AwardRulePackage`, catalog validation/fingerprint tests, VUCC/CQ selection modules | implemented locally; official eligibility remains external |
| CQ DX Marathon | Edition/category/entity-limit/profile selection and JSON export, `tst_cqmarathon*` | implemented locally; official submission remains external |
| External claims import/reconciliation | Account/category/provider checks, credited/rejected QSO IDs, Awards UI and award tests | implemented locally; provider result remains external |
| Migration matrix | Anonymized ADIF/CSV fixture matrix, N1MM SQLite tests, `tst_migration` | implemented with fixtures; real exports pending |
| N126-012/013/014 operational checks | Diagnostics UI, structured JSON evidence, operational matrix and release gate | automatic checks implemented; UI/hardware/external rows pending |
| Local release confidence | `scripts/release-gate.sh`, version gate, full build, smoke screenshot, CTest 75/75 | verified on macOS arm64 |

The matrix must be updated when a real export, desktop session, radio test or
external award/provider result is supplied. Until then, the pending labels are
deliberate and prevent the local suite from being presented as cross-platform
or official operational certification.
