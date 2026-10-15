# DXL-010 — versioned declarative award rules

DXL-010 is implemented by `AwardRulePackage` and `AwardRuleCatalog`. A club
award can be loaded from JSON without changing the core. The schema records
source, version, validity interval, supported key field, target/total and a
SHA-256 fingerprint. Duplicate versions with different content and overlapping
validity intervals are rejected.

Packages are data-only. The parser accepts a fixed allowlist of key fields and
does not expose callbacks, arbitrary paths, network access, TX control, or
code execution. Historical claims retain their rule version and fingerprint,
so a later package does not silently rewrite an earlier claim.

Verification:

```text
cmake --build build --parallel 4 --target tst_awards
ctest --test-dir build -R '^awards$' --output-on-failure
1/1 Test #7: awards ... Passed
```

The test covers a club package, persistence, duplicate/overlap rejection,
unsupported fields, unsafe versions and use by the calculator. External club
rules are not certified by this local test; their source and validity dates
remain an operator/auditor responsibility.
