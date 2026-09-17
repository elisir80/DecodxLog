#!/usr/bin/env bash
# DecoLog — crea una cartella autonoma, avviabile con doppio clic (Windows, MSYS2).
#
# Come in DecoRTTY, windeployqt da solo non basta. Qui si aggiunge:
#   * le librerie del compilatore MinGW e di terze parti (qtkeychain, OpenSSL,
#     ICU...), risolte ricorsivamente con ldd perche' anche le DLL copiate ne
#     hanno di proprie, plugin QML compresi;
#   * il plugin TLS: senza, QRZ.com e HamQTH (HTTPS) non rispondono, e il
#     programma non da' altro segno che "connessione fallita";
#   * il driver SQLite, senza il quale il log non si apre;
#   * l'albero dei moduli QML, perche' l'analisi degli import salta qualche
#     sottomodulo (QtQuick.Controls.impl, QtQuick.Dialogs.quickimpl);
#   * un qt.conf, senza il quale Qt cerca plugin e moduli dov'era installato.
#
#   scripts/deploy.sh            compila in build/ e prepara dist/
#   DIST=/c/tmp/decolog scripts/deploy.sh
set -e

MINGW=${MINGW:-/c/msys64/mingw64}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-$ROOT/build}
DIST=${DIST:-$ROOT/dist}

export PATH="$MINGW/bin:$PATH"

echo "== build =="
cmake --build "$BUILD"

echo "== cartella $DIST =="
# Si svuota invece di rimuoverla: su Windows basta una shell aperta li' dentro
# perche' la rimozione fallisca in silenzio.
mkdir -p "$DIST"
find "$DIST" -mindepth 1 -maxdepth 1 -exec rm -rf {} + 2>/dev/null || true
cp "$BUILD"/decolog.exe "$BUILD"/decolog_udpsend.exe "$DIST"/

echo "== librerie Qt =="
( cd "$DIST" && windeployqt --qmldir "$ROOT/qml" --qmldir "$ROOT/libs/decodium-ui/qml" --release \
    --no-translations --no-system-d3d-compiler --no-opengl-sw \
    decolog.exe >/dev/null )

echo "== plugin TLS e SQLite =="
mkdir -p "$DIST/tls" "$DIST/sqldrivers"
cp "$MINGW"/share/qt6/plugins/tls/*.dll "$DIST/tls/"
cp "$MINGW/share/qt6/plugins/sqldrivers/qsqlite.dll" "$DIST/sqldrivers/"
# Gli altri driver SQL trascinerebbero client MySQL, PostgreSQL e ODBC.
find "$DIST/sqldrivers" -name "*.dll" ! -name "qsqlite.dll" -delete

echo "== moduli QML completi =="
rm -rf "$DIST/qml"
cp -r "$MINGW/share/qt6/qml" "$DIST/qml"
# I moduli dell'applicazione sono dentro l'eseguibile; gli strumenti per Qt
# Designer e i moduli 3D e multimediali non servono a un log.
rm -rf "$DIST/qml/DecoLog" "$DIST/qml/Decodium" "$DIST/qml/QtQuick/Controls/designer" \
       "$DIST/qml/QtQuick3D" "$DIST/qml/QtMultimedia" "$DIST/qml/QtWebEngine" "$DIST/qml/QtWebView"

cat > "$DIST/qt.conf" <<'EOF'
[Paths]
Prefix = .
Plugins = .
Imports = qml
Qml2Imports = qml
EOF

echo "== dipendenze, ricorsivamente =="
added=1
round=0
while [ "$added" -gt 0 ] && [ "$round" -lt 10 ]; do
    added=0
    round=$((round + 1))
    while read -r f; do
        while read -r dep; do
            [ -n "$dep" ] || continue
            base=$(basename "$dep")
            if [ ! -f "$DIST/$base" ]; then
                cp "$dep" "$DIST/"
                echo "  + $base"
                added=$((added + 1))
            fi
        done < <(ldd "$f" 2>/dev/null | grep -oiE "$MINGW/bin/[^ ]+\.dll")
    done < <(find "$DIST" -type f \( -name "*.dll" -o -name "*.exe" \))
done

# OpenSSL non e' una dipendenza dichiarata: il plugin TLS lo carica a runtime,
# quindi ldd non lo vede.
for ssl in "$MINGW"/bin/libssl-3-x64.dll "$MINGW"/bin/libcrypto-3-x64.dll; do
    [ -f "$ssl" ] && cp -n "$ssl" "$DIST/"
done

echo "== licenze =="
cp "$ROOT/LICENSE" "$DIST/LICENSE.txt"
cp "$ROOT/resources/cty/COPYRIGHT.txt" "$DIST/cty.csv-COPYRIGHT.txt"

echo
echo "pronto: $DIST"
echo "  decolog.exe          l'applicazione"
echo "  decolog_udpsend.exe  finge di essere Decodium, per le prove"
du -sh "$DIST"
