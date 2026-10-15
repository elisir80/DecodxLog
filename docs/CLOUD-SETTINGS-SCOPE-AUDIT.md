# DXL-006 — Cloud settings scope audit

Audit date: 2026-10-16

## Verified behavior

The portable-settings contract is versioned (`cloudsettings::kScopeVersion`)
and separates portable, station, and device values. Device-local values are
excluded from collection and ignored on apply: serial/audio devices, rig and
network ports, local hosts, paths, backup locations, credentials, and the UDP
identity. Station profiles are transported by UUID rather than by the local
row number. Themes, CW macros, saved filters, and other portable settings
remain synchronizable.

Remote settings are applied with the last agreed base. A local value changed
since that base is kept and reported for the next push, so a second computer
cannot silently overwrite an unsent local edit. The dry-run path counts the
would-be changes without writing them. The core also exposes a deterministic
`cloudsettings::preview()` list with key, scope, action, and local/remote
values; it excludes machine-only settings and marks local conflicts as
`keep-local`.

## Evidence

`tests/tst_cloudsettings.cpp` covers the scope policy, Windows paths, serial
and audio devices, UDP identity, portable filters, profile UUID translation,
local conflict retention, canonical values, and dry-run behavior.

The focused `cloudsettings` test passed in the existing local verification
set. The current release gate also runs the complete desktop CTest suite.

## Remaining limitation

The Cloud pull now stages remote settings instead of applying them silently.
`pendingSettings` is exposed to QML with Apply and Discard actions, and the
UI lists each pending key with its scope and action, and the settings are
written only after Apply. The focused C++ and QML compilation/tests pass;
real two-device Cloud confirmation remains a separate integration check.
