# Declarative award rule packages

Award packages use the fixed format `decodxlog-award-rule-1`. A package has
an identifier, source, version, validity interval, one supported key field and
bounded target/total values. The catalog rejects invalid identifiers,
unsupported fields, invalid intervals, duplicate IDs and overlapping versions.

Packages contain data only. They have no executable code, TX operation,
network client or filesystem callback. The calculator records the package
fingerprint and rule version in the result/claim path, so a later regulation
version does not silently rewrite the meaning of an historical claim.

The accepted key fields are currently `DXCC`, `STATE`, `CQZ`, `GRID4`, `BAND`
and `CALL_PREFIX`. Adding a new rule family requires an explicit schema/test
change rather than silently interpreting arbitrary JSON.

Coverage: `tst_awards` validates a club package, persistence through a
directory, duplicate/overlap rejection, unsupported fields and unsafe version
values. It does not certify the external rules of any particular club; the
package source and validity dates remain operator/auditor responsibility.

