# Integration diagnostics audit

The Integration diagnostics panel gives a local, operator-readable path for
the main pipeline: receive, ADIF parse, duplicate protection, local save, and
QSL readiness. It does not require packet inspection.

The diagnostic ADIF marker (`APP_DECOLOG_DIAGNOSTIC=1`) is rejected before
database insertion. The marker is also covered through the local HTTP API, so
support checks cannot create a QSO in the real log. Diagnostic mode never
uploads to QSL services or relays a public contact.

The support report is shown before saving and is written only after explicit
operator approval. Secret redaction covers API keys, passwords, bearer tokens
and the local API token. The report contains statuses and safe explanatory
text, not raw packets or credentials.

Verified locally:

- isolated synthetic QSO is rejected and absent from the database;
- local API diagnostic POST returns `409` and leaves no diagnostic QSO;
- receive/parse/deduplicate/save/QSL readiness checks are exposed in the UI;
- diagnostic report redaction tests pass;
- no public QSL or relay request is made by the diagnostic path.

Hardware-specific audio, CAT and remote-service failures still require the
operator to run the relevant integration while watching these stages; this
report intentionally does not claim a remote service is reachable merely from
stored credentials.

## Verification record — 15 October 2026

On the current macOS arm64 checkout, the focused verification completed with:

```text
cmake --build build --parallel 4 --target tst_diagnostics tst_localapi
ctest --test-dir build --output-on-failure -R '^(diagnostics|localapi)$'
2/2 tests passed
```

This proves the redaction and diagnostic-marker unit behavior and the local
API isolation path. It does not prove a live radio, CAT, UDP, QSL provider, or
remote Decodium connection; those remain operator/hardware integration checks.
