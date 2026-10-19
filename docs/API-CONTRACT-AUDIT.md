# Local API contract audit

The local API is versioned under `/api/v1`, disabled by default, bound only to
`127.0.0.1`, and requires the generated token on every request. Supported
authentication forms are the documented header, Bearer token, and query-token
compatibility form.

The contract now includes `Idempotency-Key` for POST `/api/v1/qso`: retries with
the same key and body reuse the original response, while reuse with a different
body returns `409`. The normal database duplicate check remains active, so
distinct contacts are not collapsed. Idempotency records are persisted with a
30-day retention and the first stored response is immutable; a later writer
cannot overwrite it. They are not credentials and are not written to
diagnostics.

Verified locally:

- token generation and all authentication forms;
- unauthorized requests and localhost-only binding;
- fragmented HTTP request bodies;
- local API test suite and database duplicate behavior;
- persistence across database reopen, expiry cleanup and first-writer collision;
- diagnostic secret redaction tests.

No external API or network exposure was added by this change.

Verification repeated on 2026-10-18: `localapi`, `diagnostics` and `database`
passed; database coverage includes reopen, expiry cleanup and collision tests.
This verifies the v1 contract, token redaction, localhost binding and
idempotent QSO behavior with local test servers; it does not constitute a
security audit of arbitrary third-party clients.
