# Award claim lifecycle

DecoDXLog keeps four distinct facts separate:

1. a QSO may be worked and confirmed by LoTW/eQSL/card;
2. selected QSO IDs can be saved in a claim with a rule version;
3. the claim can move from `Draft` to `Submitted`, then `Verified` or
   `Rejected`;
4. only an explicitly verified claim can move to `Credited`.

A service confirmation does not create a claim or grant external credit. The
claim stores its QSO IDs, rule version, timestamps, outcome and evidence in the
local log. No automatic DXKeeper or award-provider access is assumed: external
submission and decision evidence must be supplied through the application
workflow or a documented integration.

`tst_awards` covers valid and invalid transitions, persistence, rejection and
round-tripping of the submitted QSO IDs. It does not certify a third-party
award authority or replace its official application process.

Verification on 2026-10-15: `ctest --test-dir build -R '^awards$'
--output-on-failure` passed (1/1). No undocumented automatic submission or
credit integration is enabled; this is intentional until a provider documents
an API/export contract that can be verified.
