# DXL-009 — documentation and capabilities audit

The package version is declared once in `CMakeLists.txt` and is checked by
`scripts/check-doc-version.sh` against the README, feature list, and API
example. The API example and feature list match the current `1.17.24`
contract.

The CTC neural CW decoder is explicitly optional in both build options and
feature documentation: it requires ONNX Runtime and
`DECODXLOG_CTC_ONNX=ON`; standard binaries do not claim to include it.

The documentation gate checks the version and the optional-component wording.
It does not replace a clean installation test on every operating system.

The CMake configure step now generates `build/decodxlog-capabilities.json` from
the actual build options. It records version, platform, architecture, and the
availability of the optional CTC/ONNX decoder. `scripts/check-doc-version.sh`
requires this manifest and checks that its version matches the configured
project version. This prevents a binary from advertising the neural decoder
when it was not built with ONNX Runtime.

Verification on 2026-10-16: the current macOS configuration generated the
manifest with version `1.17.24` and `cw_decoder_ctc_onnx: false`; the
documentation gate and `git diff --check` passed. A clean install and
equivalent build checks on Windows, Linux, and macOS Intel remain outside this
local environment.
