# Local API contract audit

The local API is versioned under `/api/v1`, disabled by default, bound only to
`127.0.0.1`, and requires the generated token on every request. Supported
authentication forms are the documented header, Bearer token, and query-token
compatibility form.

The contract now includes `Idempotency-Key` for POST `/api/v1/qso`: retries with
the same key and body reuse the original response, while reuse with a different
body returns `409`. The normal database duplicate check remains active, so
distinct contacts are not collapsed. Keys are process-local and bounded to 512
entries; they are not persisted as credentials or written to diagnostics.

Verified locally:

- token generation and all authentication forms;
- unauthorized requests and localhost-only binding;
- fragmented HTTP request bodies;
- local API test suite and database duplicate behavior;
- diagnostic secret redaction tests.

No external API or network exposure was added by this change.

Verification repeated on 2026-10-15: `localapi` and `diagnostics` passed 2/2.
This verifies the v1 contract, token redaction, localhost binding and
idempotent QSO behavior with local test servers; it does not constitute a
security audit of arbitrary third-party clients.
