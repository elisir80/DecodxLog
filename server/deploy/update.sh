#!/usr/bin/env bash
# DecoLog Cloud — aggiornamento del servizio gia' in piedi.
#
#   cd /srv/decolog && git pull && sudo bash server/deploy/update.sh
#
# Ricopia il codice, aggiorna le dipendenze e riavvia. Se il servizio non torna
# su, rimette quello di prima e lo dice: un aggiornamento non deve lasciare la
# stazione senza sync.

set -euo pipefail

SERVICE_NAME=decolog-cloud
APP_DIR=/opt/decolog-cloud
here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

[[ $EUID -eq 0 ]] || { echo "Serve sudo."; exit 1; }
[[ -d $APP_DIR/.venv ]] || { echo "Il servizio non e' installato: usa install.sh"; exit 1; }

backup=$(mktemp -d)
cp -a "$APP_DIR/decolog_cloud" "$backup/"

rsync -a --delete --exclude '__pycache__' "$here/decolog_cloud" "$here/requirements.txt" "$APP_DIR/"
"$APP_DIR/.venv/bin/pip" install --quiet -r "$APP_DIR/requirements.txt"
chown -R decolog:decolog "$APP_DIR"

systemctl restart "$SERVICE_NAME"
sleep 2
if systemctl is-active --quiet "$SERVICE_NAME" && curl -fsS http://127.0.0.1:8788/v1/health >/dev/null; then
    rm -rf "$backup"
    echo "Aggiornato: $(systemctl show -p ActiveEnterTimestamp --value "$SERVICE_NAME")"
else
    echo "Il servizio non e' tornato su: rimetto la versione di prima."
    rsync -a --delete "$backup/decolog_cloud" "$APP_DIR/"
    chown -R decolog:decolog "$APP_DIR"
    systemctl restart "$SERVICE_NAME"
    journalctl -u "$SERVICE_NAME" -n 40 --no-pager
    exit 1
fi
