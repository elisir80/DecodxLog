#!/usr/bin/env bash
# DecoDXLog Cloud — aggiornamento del servizio gia' in piedi.
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

# "Vivo" non basta: anche la versione vecchia risponde "ok". /v1/health dice
# anche cosa sa fare, e qui si controlla che il codice in funzione sia davvero
# quello appena copiato — un aggiornamento a meta' e' peggio di nessuno.
health=$(curl -fsS http://127.0.0.1:8788/v1/health 2>/dev/null || true)
# Quello che deve esserci perche' il codice sia davvero quello nuovo: si alza a
# ogni versione che aggiunge qualcosa a /v1/health.
wanted='"approval"'

if systemctl is-active --quiet "$SERVICE_NAME" && [[ $health == *'"status":"ok"'* ]] \
   && [[ $health == *"$wanted"* ]]; then
    rm -rf "$backup"
    echo "Aggiornato: $(systemctl show -p ActiveEnterTimestamp --value "$SERVICE_NAME")"
    echo "In funzione: $health"
else
    if [[ -n $health ]]; then
        echo "Il servizio risponde ma non e' la versione nuova: $health"
    else
        echo "Il servizio non e' tornato su: rimetto la versione di prima."
    fi
    rsync -a --delete "$backup/decolog_cloud" "$APP_DIR/"
    chown -R decolog:decolog "$APP_DIR"
    systemctl restart "$SERVICE_NAME"
    journalctl -u "$SERVICE_NAME" -n 40 --no-pager
    exit 1
fi
