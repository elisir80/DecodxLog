#!/usr/bin/env bash
# Gate comune per i workflow di pacchetto: nessun artefatto puo' essere
# preparato se il build corrente non supera i test e non produce l'eseguibile.
set -euo pipefail

BUILD_DIR="${1:?build directory required}"
EXECUTABLE="${2:?executable path required}"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
"$ROOT_DIR/scripts/check-doc-version.sh" "$ROOT_DIR"

cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure
test -x "$EXECUTABLE"
echo "release gate passed: $EXECUTABLE"
