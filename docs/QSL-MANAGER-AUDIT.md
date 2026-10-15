# QSL manager audit

The QSO detail view shows a recorded `QSL_VIA` manager together with its
source and acquisition time. Data obtained from a callbook is marked with its
source; entries older than 180 days are flagged for rechecking.

The suggestion also lists the confirmation services that are still
unconfirmed for that QSO. This is a potential-credit hint only: it does not
claim that a manager will answer or that an award rule will accept the card.
The operator must review the route and approve any request; automatic sending
is explicitly disabled by the controller contract.

Verified locally:

- manager source and acquisition timestamp are exposed;
- stale manager data is warned about;
- potential unconfirmed services are reported;
- the suggestion requires approval and never sends automatically;
- database, QSL and QML popup tests pass.

Verification repeated on 2026-10-16: `database`, `qsl` and `qmlpopups`
passed 3/3. The controller exposes `potentialCredits`, `source`,
`acquiredAt`, `stale`, `requiresApproval` and `automaticSend=false`; the UI
uses those values for the operator-facing warning.
