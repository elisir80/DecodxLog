# Official and personal award scopes — audit DXL-003

The default `AwardFilter` is the official profile for the ARRL-style DXCC,
DXCC Challenge and WAS calculations: `count60m` is false. A user may enable a
personal 60 m view, which is labelled `personal` in the awards UI and is not
used as official credit.

The calculator keeps QSOs in the log regardless of award eligibility. Official
credit sources are exposed separately from user overrides; for DXCC/WAS the
default is LoTW and paper card, while non-official choices such as eQSL or QRZ
are visible as an operator-selected estimate rather than silently presented as
ARRL credit.

The UI exposes the rule source, official/personal status and confirmation
sources in the result data. Tests in `tst_awards` cover the default 60 m
exclusion, the personal override, source overrides and retention of the QSO.

Verification:

```text
cmake --build build --parallel 4 --target tst_awards
ctest --test-dir build -R '^awards$' --output-on-failure
```

Result: the focused C++ award test passed after changing the new-profile
default to official rules; the personal override and QSO retention cases also
pass. The Cloud analytics suite currently passes 136/136 tests. Cloud
deployment remains outside this local verification.

The ARRL [DXCC rules](https://www.arrl.org/dxcc-rules) state that 60 m contacts
do not qualify for DXCC credit; the [DXCC Challenge rules](https://www.arrl.org/files/file/DXCC/Rules.pdf)
also exclude 60 m. Cloud parity is not independently executable from this
checkout and remains a backend/fixture verification item.
