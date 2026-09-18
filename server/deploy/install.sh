#!/usr/bin/env bash
# DecoLog Cloud — messa in servizio su un VPS Ubuntu con nginx gia' installato.
#
# Da lanciare con sudo, sul VPS, dalla cartella del repo:
#
#   sudo bash server/deploy/install.sh
#
# Cosa fa, e niente di piu':
#   · crea l'utente di sistema `decolog` (senza login) e le sue cartelle;
#   · copia il servizio in /opt/decolog-cloud e ci fa il suo venv;
#   · se sul VPS c'e' PostgreSQL, crea database e utente dedicati; altrimenti
#     usa un file SQLite in /var/lib/decolog-cloud;
#   · installa l'unit systemd e la avvia;
#   · mette il sito nginx per cloud.ft2.it, senza toccare gli altri.
#
# Non tocca i siti che ci sono gia', non apre porte sul firewall, non installa
# Docker. Rilanciarlo e' innocuo: quello che c'e' gia' lo lascia com'e'.

set -euo pipefail

SERVICE_NAME=decolog-cloud
APP_DIR=/opt/decolog-cloud
DATA_DIR=/var/lib/decolog-cloud
ENV_FILE=/etc/decolog-cloud.env
DOMAIN=${DECOLOG_DOMAIN:-cloud.ft2.it}
PORT=8788

here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)   # la cartella server/

say() { printf '\n\033[1;36m== %s\033[0m\n' "$*"; }

[[ $EUID -eq 0 ]] || { echo "Serve sudo."; exit 1; }

say "Utente e cartelle"
id -u decolog >/dev/null 2>&1 || useradd --system --home "$DATA_DIR" --shell /usr/sbin/nologin decolog
install -d -o decolog -g decolog -m 750 "$DATA_DIR"
install -d -o root -g root -m 755 "$APP_DIR"

say "Codice e ambiente Python"
apt-get install -y --no-install-recommends python3-venv python3-dev build-essential libpq-dev >/dev/null
rsync -a --delete --exclude '.venv' --exclude '__pycache__' --exclude '*.sqlite' \
      "$here/decolog_cloud" "$here/requirements.txt" "$APP_DIR/"
[[ -d $APP_DIR/.venv ]] || python3 -m venv "$APP_DIR/.venv"
"$APP_DIR/.venv/bin/pip" install --quiet --upgrade pip
"$APP_DIR/.venv/bin/pip" install --quiet -r "$APP_DIR/requirements.txt"
chown -R decolog:decolog "$APP_DIR"

say "Database"
if [[ -f $ENV_FILE ]]; then
    echo "$ENV_FILE c'e' gia': lo lascio com'e'."
elif command -v psql >/dev/null 2>&1 && systemctl is-active --quiet postgresql; then
    # PostgreSQL c'e': database e utente dedicati, password fatta qui e scritta
    # solo nel file d'ambiente, che legge soltanto root.
    PGPASS=$(head -c 24 /dev/urandom | base64 | tr -d '/+=' | head -c 24)
    sudo -u postgres psql -tAc "SELECT 1 FROM pg_roles WHERE rolname='decolog'" | grep -q 1 \
        || sudo -u postgres psql -c "CREATE ROLE decolog LOGIN PASSWORD '$PGPASS'"
    sudo -u postgres psql -tAc "SELECT 1 FROM pg_database WHERE datname='decolog'" | grep -q 1 \
        || sudo -u postgres createdb -O decolog decolog
    cat > "$ENV_FILE" <<EOF
# DecoLog Cloud — configurazione del servizio. La legge solo root.
DECOLOG_DATABASE_URL=postgresql+psycopg://decolog:$PGPASS@127.0.0.1/decolog
# A 1 chiunque puo' farsi un account su questo server. Dopo esserti registrato
# la prima volta, mettilo a 0 e riavvia: nessun altro entra.
DECOLOG_ALLOW_SIGNUP=1
EOF
    echo "PostgreSQL: database e utente 'decolog' pronti."
else
    cat > "$ENV_FILE" <<EOF
# DecoLog Cloud — configurazione del servizio. La legge solo root.
DECOLOG_DATABASE_URL=sqlite:///$DATA_DIR/decolog-cloud.sqlite
DECOLOG_ALLOW_SIGNUP=1
EOF
    echo "Niente PostgreSQL attivo: uso SQLite in $DATA_DIR."
fi
chmod 640 "$ENV_FILE"
chown root:decolog "$ENV_FILE"

say "Servizio systemd"
install -m 644 "$here/deploy/$SERVICE_NAME.service" "/etc/systemd/system/$SERVICE_NAME.service"
systemctl daemon-reload
systemctl enable --now "$SERVICE_NAME"
sleep 2
systemctl is-active --quiet "$SERVICE_NAME" || { journalctl -u "$SERVICE_NAME" -n 30 --no-pager; exit 1; }
curl -fsS "http://127.0.0.1:$PORT/v1/health" && echo

say "Sito nginx per $DOMAIN"
site=/etc/nginx/sites-available/$DOMAIN
if [[ -f $site ]]; then
    echo "$site c'e' gia': non lo tocco."
else
    sed "s/cloud\.ft2\.it/$DOMAIN/g" "$here/deploy/nginx-cloud.ft2.it.conf" > "$site"
    ln -sfn "$site" "/etc/nginx/sites-enabled/$DOMAIN"
    # Finche' non c'e' il certificato, il blocco 443 non sta in piedi: si parte
    # in HTTP e ci pensa certbot a riscrivere il file.
    sed -i '/listen 443 ssl;/,$ d' "$site"
    printf '}\n' >> "$site"
    sed -i 's|return 301 https://\$host\$request_uri;|proxy_pass http://127.0.0.1:'"$PORT"';\n        proxy_set_header Host \$host;\n        proxy_set_header X-Forwarded-Proto \$scheme;|' "$site"
    nginx -t && systemctl reload nginx
fi

say "Fatto"
cat <<EOF
Il servizio gira su 127.0.0.1:$PORT e risponde su http://$DOMAIN.

Ora, una volta sola:

  1. il DNS di $DOMAIN deve puntare a questo VPS (record A);
  2. certbot --nginx -d $DOMAIN        # mette il certificato e passa a HTTPS
  3. in DecoLog: Impostazioni -> Sync e Cloud, server https://$DOMAIN,
     nominativo e password, "Crea l'account";
  4. poi in $ENV_FILE metti DECOLOG_ALLOW_SIGNUP=0 e
     systemctl restart $SERVICE_NAME, cosi' il server resta tuo.

Aggiornamenti: server/deploy/update.sh
Registro:     journalctl -u $SERVICE_NAME -f
EOF
