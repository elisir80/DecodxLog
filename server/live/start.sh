#!/usr/bin/env bash
# DecoDXLog Live — accende lo schermo finto e ci mette sopra DecoDXLog.
#
# Tre pezzi, in fila: Xvfb fa lo schermo, x11vnc lo pubblica, websockify/noVNC lo
# porta nel browser. Se DecoDXLog si chiude, si chiude tutto: cosi' il container
# muore e chi lo sorveglia (docker, systemd) lo rialza, invece di lasciare in
# piedi uno schermo vuoto.
set -euo pipefail

screen="${DECOLOG_SCREEN:-1600x900x24}"
display="${DISPLAY:-:99}"
home="${DECOLOG_HOME:-/home/decolog}"
db="${DECOLOG_DB:-$home/decodxlog.sqlite}"

mkdir -p "$home"

# La password del VNC: senza, la scrivania sarebbe di chiunque passi.
if [[ -n "${DECOLOG_VNC_PASSWORD:-}" ]]; then
    x11vnc -storepasswd "$DECOLOG_VNC_PASSWORD" "$home/.vncpass" >/dev/null 2>&1
    auth=(-rfbauth "$home/.vncpass")
else
    echo "DECOLOG_VNC_PASSWORD non impostata: mi fermo, una scrivania aperta non si lascia." >&2
    exit 1
fi

cleanup() { kill 0 2>/dev/null || true; }
trap cleanup EXIT

Xvfb "$display" -screen 0 "$screen" -nolisten tcp &
for _ in $(seq 1 50); do
    xdpyinfo -display "$display" >/dev/null 2>&1 && break
    sleep 0.1
done

# Un gestore di finestre minimo: senza, i dialoghi di DecoDXLog non si spostano.
openbox --sm-disable &

# Solo da dentro il container: fuori ci arriva noVNC, e davanti c'e' nginx.
x11vnc -display "$display" -forever -shared -localhost -quiet "${auth[@]}" &

websockify --web /usr/share/novnc 6080 localhost:5900 &

# E adesso il programma: quello di GitHub, compilato com'e'.
decolog --db "$db" "$@"
