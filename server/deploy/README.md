# DecoDXLog Cloud sul VPS

Il servizio sta accanto agli altri siti del VPS, dietro il nginx che c'e' gia':
niente Docker da installare, un servizio systemd che parte da solo e si aggiorna
con due comandi.

```
Internet ──HTTPS──> nginx (cloud.ft2.it) ──> 127.0.0.1:8788 ──> uvicorn (utente decolog)
                                                                      │
                                                            PostgreSQL o SQLite
```

## Prima di cominciare

1. **DNS**: un record `A` per `cloud.ft2.it` verso l'indirizzo del VPS
   (`57.128.222.68`). Senza quello certbot non puo' fare il certificato.
2. **Accesso SSH** al VPS con un utente che possa usare `sudo`.

## Installazione

Sul VPS, una volta sola:

```bash
git clone https://github.com/iu8lmc/decolog.git /srv/decolog     # o copia la cartella server/
cd /srv/decolog
sudo bash server/deploy/install.sh
```

Lo script:

- crea l'utente di sistema `decolog`, senza login;
- mette il servizio in `/opt/decolog-cloud` con il suo ambiente Python;
- se sul VPS c'e' PostgreSQL attivo crea **database e utente dedicati** con una
  password generata al momento, altrimenti usa un file SQLite in
  `/var/lib/decolog-cloud`;
- scrive `/etc/decolog-cloud.env` (lo legge solo root);
- installa e avvia l'unit systemd, e controlla che risponda;
- aggiunge il sito nginx per `cloud.ft2.it` **senza toccare gli altri**.

Poi il certificato:

```bash
sudo certbot --nginx -d cloud.ft2.it
```

## Il log dal browser

Finito il certificato, `https://cloud.ft2.it` apre la pagina di accesso: stessi
nominativo e password del programma. Non serve altro — le pagine le serve lo
stesso servizio, e nginx le passa gia'.

## Il primo account

In DecoDXLog: **Impostazioni → Sync e Cloud**, server `https://cloud.ft2.it`,
nominativo e password, **Crea l'account**.

## Chi entra, lo decidi tu

La porta resta aperta — chiudere `DECOLOG_ALLOW_SIGNUP` vuol dire che chi
arriva trova un rifiuto secco e non si capisce perche'. Chi si registra ottiene
il suo token, ma l'account nasce **in attesa**: il sync gli risponde che la
registrazione aspetta l'approvazione, e lo dice con quelle parole.

A ogni registrazione arriva una email a `DECOLOG_NOTIFY_EMAIL` con il
nominativo, l'ora e l'indirizzo da cui e' arrivata, e un collegamento. Il
collegamento apre una pagina con due pulsanti: **fallo entrare** oppure
**rifiuta e cancella**. Vale una volta sola. Si fa dal telefono, senza SSH.

Le variabili della posta stanno in `/etc/decolog-cloud.env`: sono le stesse
credenziali Gmail della community.

```
DECOLOG_NOTIFY_EMAIL=iu8lmc@gmail.com
DECOLOG_SMTP_USER=...
DECOLOG_SMTP_PASSWORD=...
DECOLOG_PUBLIC_URL=https://cloud.ft2.it
```

Rifiutare **cancella** l'account: chi e' stato rifiutato per sbaglio si
registra di nuovo e arriva un altro avviso. Chi aveva gia' un account prima di
questa versione resta dentro: la migrazione li segna tutti approvati.

Per tornare a com'era — chiunque si registra ed entra subito — si mette
`DECOLOG_APPROVAL=0`.

Chi aspetta ancora, da riga di comando:

```bash
sudo -u decolog /opt/decolog-cloud/.venv/bin/python -c   "from decolog_cloud import approval, models;    db = models.SessionLocal();    print([(a.callsign, a.id, a.approval_token) for a in approval.pending(db)])"
```

## Ogni giorno

| Cosa | Comando |
|---|---|
| Come sta | `systemctl status decolog-cloud` |
| Cosa dice | `journalctl -u decolog-cloud -f` |
| Aggiornarlo | `cd /srv/decolog && git pull && sudo bash server/deploy/update.sh` |
| Quanti QSO | `curl -s https://cloud.ft2.it/v1/health` e, con il token, `/v1/sync/status` |

`update.sh` rimette la versione di prima se il servizio non torna su: un
aggiornamento non deve lasciare la stazione senza sync.

## Backup

Il log vero resta sul computer di casa, quindi il VPS non e' l'unica copia. Vale
comunque la pena mettere il database nel backup notturno che c'e' gia':

```bash
# PostgreSQL
sudo -u postgres pg_dump decolog | gzip > /var/backups/decolog-$(date +%F).sql.gz
# SQLite
sudo sqlite3 /var/lib/decolog-cloud/decolog-cloud.sqlite ".backup '/var/backups/decolog-$(date +%F).sqlite'"
```

## Sicurezza

- Il token viaggia nell'intestazione `Authorization`: **solo HTTPS** fuori casa.
- Le password stanno con Argon2, dei token resta solo l'impronta.
- Il servizio gira senza privilegi, con `ProtectSystem=strict`: puo' scrivere
  soltanto nella sua cartella dati.
- `/docs` (la pagina dell'API) risponde solo da dentro la macchina.
- La porta 8788 non e' aperta sul firewall: ci arriva solo nginx.
