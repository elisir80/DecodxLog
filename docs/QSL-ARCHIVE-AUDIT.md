# DXL-016 — received QSL archive audit

Received QSL/eQSL image data is stored in the local `qsl_attachment` table,
deduplicated by SHA-256, and linked through `qsl_attachment_qso`. One image can
therefore document multiple QSOs without storing duplicate bytes. The
attachment remains inside the private local database and is not exposed by the
award calculator or converted into a confirmation automatically.

The station bundle/restore path copies the SQLite database atomically, so the
attachment bytes and links are included. The restore test now verifies the
attachment content and QSO association after reopening the restored database.

Verification:

```text
cmake --build build --parallel 4 --target tst_logbackup
ctest --test-dir build -R '^logbackup$' --output-on-failure
1/1 Test #36: logbackup ... Passed

Re-verified on 2026-10-15: `qsl` and `logbackup` passed 2/2. The focused
tests cover SHA-256 deduplication, one attachment linked to multiple QSOs,
byte recovery after restore, and the fact that an attachment does not create
an award confirmation.
```

The test is local and temporary-directory based; it does not replace a
cross-device backup/restore exercise.
