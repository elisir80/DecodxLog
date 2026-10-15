# DXL-008 — macOS distribution audit

## Current state

The macOS workflow deploys the Qt bundle, applies an ad-hoc bundle signature,
and verifies that signature with `codesign --verify --deep --strict`. The
automatic-update manifest is signed separately by `decodxlog_sign` using the
repository signing key secret. These are two different trust layers.

## Not complete

The workflow does not yet have a Developer ID Application certificate,
Developer ID Installer path, or `notarytool` submission/staple step. The
current ad-hoc signature therefore cannot prove installation on a clean Mac
under normal Gatekeeper policy, and no claim of notarization is made.

Completing this item requires repository secrets and Apple account/team
authorization supplied by the project owner, followed by a real clean-machine
install test. The required evidence is:

1. `codesign --display --verbose=4` and `codesign --verify --deep --strict`
   for the final bundle signed by Developer ID.
2. Successful `xcrun notarytool submit --wait` and stapling verification.
3. A clean macOS install/open test with Gatekeeper enabled.
4. Independent verification of the update manifest signature.

The current workflow already performs item 4 and ad-hoc bundle verification;
items 1–3 remain pending credentials and an external clean machine.
