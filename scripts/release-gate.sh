#!/usr/bin/env bash
# Gate comune per i workflow di pacchetto: nessun artefatto puo' essere
# preparato se il build corrente non supera i test e non produce l'eseguibile.
set -euo pipefail

BUILD_DIR="${1:?build directory required}"
EXECUTABLE="${2:?executable path required}"
MANIFEST_FILE="${3:-$BUILD_DIR/decodxlog-capabilities.json}"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
"$ROOT_DIR/scripts/check-doc-version.sh" "$ROOT_DIR" "$MANIFEST_FILE"

cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure
test -x "$EXECUTABLE"
startup_probe_base="$(mktemp "${TMPDIR:-/tmp}/decodxlog-release-gate.XXXXXX")"
startup_probe="${startup_probe_base}.png"
trap 'rm -f "$startup_probe_base" "$startup_probe"' EXIT
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" "$EXECUTABLE" --show badge --grab "$startup_probe"
test -s "$startup_probe"
echo "startup smoke test passed: $startup_probe"
echo "release gate passed: $EXECUTABLE"
