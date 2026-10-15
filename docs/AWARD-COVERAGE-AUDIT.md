# Award coverage and official-status audit

## DXL-012 — verifica 2026-10-16

The award engine exposes worked/confirmed counters for several useful
reference families, including grids, IOTA, POTA, SOTA, WWFF and national
programs. A counter is not automatically proof of eligibility.

Every result carries `officialRules`. Results without a verified rules source
are now labelled in the Awards dialog as `statistics · rules not verified`.
The generic four-character locator result is explicitly not VUCC; VUCC rules
also require band, station/account and evidence constraints.

VUCC and CQ Marathon are not claimed as implemented awards in this build:
their official rules, responsible verifier and reproducible fixtures have not
yet been established in the project. No automatic submission or credit is
therefore exposed for them.

The current `grids` entry is deliberately only a statistical counter. The
current [ARRL VUCC rules](https://www.arrl.org/files/file/Awards/VUCC_Rules_July_2023.pdf)
and [VUCC program page](https://www.arrl.org/vucc) require per-band grid
credit, station/account constraints and review by an authorised [awards
manager](https://www.arrl.org/awards-managers); a generic grid total cannot
satisfy those requirements. The current [CQ DX Marathon rules](https://www.dxmarathon.com/rules/2026/)
also define their own country/zone lists, single-location constraints and
submission rules, so they cannot be represented by the generic counter.
Neither program is advertised as implemented until it has a dedicated data
model, verifier metadata and reproducible fixtures.

`tst_awards` covers the existing fixtures and the official/statistical
distinction; `gridCountIsExplicitlyStatisticalNotVucc` checks the source,
verifier and label, and asserts that `vucc` and `cq_marathon` are not exposed
as implemented awards. External award eligibility still requires the
responsible organization's current rules and evidence.

The test also asserts that `vucc` and `cq_marathon` are not exposed as
implemented award IDs and that `grids` remains marked statistical.
