#!/usr/bin/env bash
# DecoDXLog — firma i file di una release prima di pubblicarli.
#
# Dopo scripts/deploy.sh e scripts/installer.sh ci sono lo zip e il setup della
# versione di CMakeLists.txt. Questo script ne scrive l'elenco firmato
# (decodxlog-release.json e .sig) accanto a loro: vanno caricati sulla release
# insieme ai pacchetti, se no l'aggiornamento automatico non li installa.
#
#   scripts/sign-release.sh
#   BUILD=/c/decolog/build-rel scripts/sign-release.sh
#   KEY_FILE=/e/chiave-di-scorta.txt scripts/sign-release.sh   (con la chiave di scorta)
#
# La chiave segreta sta nel portachiavi (decodxlog_sign keygen): qui non si
# legge, non si stampa e non si copia.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-$ROOT/build-rel}
REPOSITORY=${REPOSITORY:-iu8lmc/DecoDXLog}
TOOL="$BUILD/decodxlog_sign"
[ -x "$TOOL.exe" ] && TOOL="$TOOL.exe"
if [ ! -x "$TOOL" ]; then
    echo "manca $TOOL: si compila con DECODXLOG_BUILD_TOOLS=ON" >&2
    exit 1
fi

VERSION=$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\{1,\}\([0-9][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)
cd "$ROOT"
FILES=()
for f in "DecoDXLog-$VERSION-setup.exe" "DecoDXLog-$VERSION-win64.zip"; do
    [ -f "$f" ] && FILES+=("$f")
done
if [ ${#FILES[@]} -eq 0 ]; then
    echo "nessun pacchetto della $VERSION da firmare" >&2
    exit 1
fi

KEY=(--name main)
[ -n "${KEY_FILE:-}" ] && KEY=(--key-file "$KEY_FILE")

echo "== firma $REPOSITORY $VERSION =="
"$TOOL" sign --repository "$REPOSITORY" --version "$VERSION" "${KEY[@]}" --out "$ROOT" "${FILES[@]}"
echo "da caricare insieme ai pacchetti: decodxlog-release.json decodxlog-release.json.sig"
