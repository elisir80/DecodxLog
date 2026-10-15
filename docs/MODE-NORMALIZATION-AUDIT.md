# DXL-005 — Mode normalization audit

## Scope

The desktop log, diploma/statistics grouping, and ADIF import must agree on
the canonical groups where they share that semantic: `CW`, `PHONE`, and
`DATA`. Contest rule classification is intentionally separate because contest
categories are rule-specific rather than a general log classification.

## Verified behavior

`Adif::modeGroup()` and `modes::groupFor()` now agree for CW variants,
including `CW`, `CWR`, and `CW-R`. Voice variants include SSB/USB/LSB,
AM/FM, DMR, and C4FM. RTTY and FT2/FT4/FT8 remain data modes. Unknown modes
remain `DATA`; they are not silently rewritten to a known mode. ADIF image
modes such as SSTV retain their dedicated `IMAGE` group.

The regression fixture is in `tests/tst_lotw.cpp` and is executed by the
`lotw` CTest target. The existing `tests/tst_modes.cpp` covers the diploma and
statistics classifier independently, including an explicit cross-check against
`adif::modeGroup()` for CW, voice, data and unknown modes.

## Evidence

```text
cmake --build build --parallel 4 --target tst_lotw
ctest --test-dir build -R '^lotw$' --output-on-failure
1/1 Test #17: modes ... Passed
1/1 Test #9: lotw ... Passed
```

This is a local macOS arm64 verification. Cross-platform builds remain part
of the release gate and are not claimed by this audit.
