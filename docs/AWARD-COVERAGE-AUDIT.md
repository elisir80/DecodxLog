# Award coverage and official-status audit

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
current ARRL VUCC rules require per-band grid credit, station/account
constraints and confirmation/application review by an authorised awards
manager; a generic grid total cannot satisfy those requirements. The source
used for this decision is the [ARRL VUCC rules PDF](https://www.arrl.org/files/file/Awards/VUCC_Rules.pdf)
and the [ARRL VUCC program page](https://www.arrl.org/vucc). No CQ Marathon
implementation is advertised until its official rules and verifier are
available to the project.

`tst_awards` covers the existing fixtures and the official/statistical
distinction; external award eligibility still requires the responsible
organization's current rules and evidence.

The test also asserts that `vucc` and `cq_marathon` are not exposed as
implemented award IDs and that `grids` remains marked statistical.
