#!/usr/bin/env bash
# DecoDXLog — l'installatore per Windows.
#
# Prima prepara la cartella (scripts/deploy.sh), poi la impacchetta con Inno
# Setup. Esce DecoDXLog-<versione>-setup.exe accanto allo zip: quello e' il file
# che l'aggiornamento automatico scarica e lancia.
#
#   scripts/installer.sh
#   BUILD=/c/decolog/build-rel scripts/installer.sh
#   NO_DEPLOY=1 scripts/installer.sh      (la cartella dist/ c'e' gia')
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
DIST=${DIST:-$ROOT/dist}
ISCC=${ISCC:-"/c/Program Files (x86)/Inno Setup 6/ISCC.exe"}

if [ ! -x "$ISCC" ] && ! command -v "$ISCC" >/dev/null; then
    echo "Inno Setup non c'e': $ISCC" >&2
    echo "Si scarica da https://jrsoftware.org/isdl.php (ISCC.exe e' il compilatore)." >&2
    exit 1
fi

if [ -z "${NO_DEPLOY:-}" ]; then
    echo "== cartella da impacchettare =="
    NO_ZIP=1 bash "$ROOT/scripts/deploy.sh"
fi

VERSION=$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\{1,\}\([0-9][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)
VERSION=${VERSION:-0.0.0}

echo "== installatore $VERSION =="
# Inno Setup vuole i percorsi come li scrive Windows.
win() { cygpath -w "$1"; }
"$ISCC" //Qp \
    "//DVersion=$VERSION" \
    "//DSource=$(win "$DIST")" \
    "//DOut=$(win "$ROOT")" \
    "$(win "$ROOT/packaging/decodxlog.iss")"

SETUP="$ROOT/DecoDXLog-$VERSION-setup.exe"
if [ -f "$SETUP" ]; then
    ls -lh "$SETUP" | awk '{print "installatore: " $9 " (" $5 ")"}'
else
    echo "l'installatore non e' uscito" >&2
    exit 1
fi
