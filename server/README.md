# DecoDXLog Cloud

Il servizio che tiene allineato il log fra i dispositivi di una stazione: il PC
dello shack, il portatile che va in portatile, domani il telefono.

Non e' un secondo logbook. Il log vero resta il file SQLite di DecoDXLog, che
funziona anche senza rete; il Cloud e' il posto dove i dispositivi si passano le
modifiche, e la copia che resta se il computer si rompe.

## Come funziona

Ogni QSO ha un `uuid` fatto dal client, una `revision` che sale a ogni modifica
e un `dirty` che dice se deve ancora partire. Da qui:

* **push** — il client manda i QSO in coda con la revisione che conosceva. Se il
  server ne ha una piu' alta, il client e' indietro e il server tiene la sua
  (`stale`). Se ne ha una uguale ma diversa nel contenuto, due dispositivi hanno
  scritto senza sapersi: **vince l'ultima modifica**, e quella che perde finisce
  nello storico (`conflict`), non nel cestino.
* **pull** — `GET /v1/sync/pull?since=<cursore>` restituisce quello che e'
  cambiato dopo quel punto. Il cursore e' il numero progressivo delle modifiche
  dell'account, non un orario: due dispositivi che scrivono nello stesso secondo
  non si perdono.
* **duplicati** — lo stesso nominativo, sulla stessa banda, nello stesso gruppo
  di modi e a meno di due minuti (dieci, se scritto a mano) e' lo stesso QSO
  anche con un altro uuid: il server lo dice al client (`duplicate`) invece di
  tenerne due.
* **cancellazioni** — viaggiano come una modifica qualsiasi (`deleted: true`):
  la riga resta, cosi' anche gli altri dispositivi la tolgono.

Il server non conosce i campi ADIF: tiene il QSO come arriva, in un documento
JSON. Il giorno che DecoDXLog impara un campo nuovo, qui non si tocca niente.

## Non solo i QSO

Un log non e' solo l'elenco dei collegamenti: chi apre DecoDXLog sul secondo
computer si deve ritrovare la stessa stazione. Oltre ai QSO viaggiano quindi i
**documenti**, con le stesse regole (revisione, ultima modifica che vince,
storico, stesso cursore):

* `profile` — un documento per profilo stazione: nominativo di stazione,
  operatore, locatore, radio, antenna, potenza, e quale e' il predefinito.
* `setting` / `station` — un documento solo con **tutte** le impostazioni: tema,
  lingua, colonne e filtri salvati del log, cluster, premi, invii automatici,
  propagazione, rotore, dedup della UDP, backup, e anche porte, percorsi e
  indirizzi dei programmi accanto. Il profilo attivo e' detto per uuid, cosi'
  vale anche dove ha un altro numero di riga.

* `secret` / `vault` — le credenziali dei servizi (QRZ, LoTW, Club Log, eQSL,
  HamQTH, HamAlert), **chiuse dal client**. Il server tiene `{"alg":
  "aes-256-gcm", "sealed": "<base64>"}` e non ha modo di aprirlo: la chiave
  nasce dalla password del Cloud (PBKDF2-HMAC-SHA256, 200.000 giri) e la
  password il server la conosce solo come impronta Argon2. Anche con il
  database in mano, senza la password dell'utente qui non c'e' niente da
  leggere. Il client puo' spegnerlo.

Quello che **non** passa di qui — e non e' una scelta di stile: il promemoria di
cosa e' salvato nel portachiavi di *quella* macchina, e il quaderno del sync
stesso (nominativo collegato, ora dell'ultimo giro).

Le impostazioni che JSON non sa dire (un filtro salvato e' un QVariant di Qt)
arrivano impacchettate: `{"__qvariant__": "<base64>"}`. La pagina Stazione le
mostra per quello che sono, senza vomitare il blob.

I documenti stanno nella stessa spinta dei QSO (`docs` in `/v1/sync/push`,
`docResults` nella risposta) e nello stesso pull: un giro solo, un cursore solo.

## Il log dal browser

Non una pagina web che parla dello stesso log: **la stessa finestra**. Barra
superiore a blocchi, tre colonne di pannelli — scheda del QSO a sinistra, log in
mezzo, scheda del nominativo con FT2 Award e mappa a destra — le cinque schede
in basso e la barra di stato. Si sceglie un QSO nel log e le colonne seguono.

Anche i colori sono quelli della stazione: il tema arriva con le impostazioni
sincronizzate (`theme/current`, la variante d'accento, la densita') e diventa le
variabili CSS della pagina — `decolog_cloud/theme.py` ha gli stessi valori di
`libs/decodium-ui/src/ThemeManager.cpp`.

| Dove | Cosa c'e' |
|---|---|
| colonna sinistra | il QSO scelto, campo per campo (`/qso/<uuid>` apre anche tutti gli ADIF) |
| colonna centrale | il log, con ricerca e filtri; `/map` mette qui la mappa grande |
| colonna destra | scheda del nominativo (gia' lavorato, bande, modi, QSL), FT2 Award, mappa |
| scheda Diplomi | `/awards` — DXCC, FT2, WAZ, WAS, WPX, locatori, IOTA, POTA, SOTA, WWFF, per banda, con quello che manca |
| scheda Statistiche | `/stats` — anni, mesi, ore UTC, bande, modi, continenti, mappa di calore banda per ora |
| scheda Invio QSL | `/qsl` — inviate e ricevute per servizio, ultime conferme |
| scheda Registro attivita' | `/activity` — cosa e' arrivato sul Cloud, da quale dispositivo |
| scheda DX Cluster | `/cluster` — le fonti e le regole d'avviso della stazione (il collegamento vive nel programma) |
| scheda Propagazione | `/propagation` — SFI, macchie, A, K, aurora, MUF e le condizioni banda, dalla stessa fonte del programma (hamqsl.com), chiesta una volta all'ora |
| Stazione | `/station` — profili, impostazioni, e se la cassaforte delle credenziali e' salita |

I conti non stanno in tabelle di riepilogo: si rifanno dai QSO a ogni richiesta,
con le stesse regole del programma (`decolog_cloud/analytics.py` e'
`src/core/Awards.cpp` portato in Python — gruppi di modi, prefisso WPX, i
cinquanta stati). Cosi' una correzione a un QSO si vede subito ovunque, e le due
facce del log dicono la stessa cosa.

Da qui si guarda e si scarica; si scrive dal programma. La sessione del browser
e' un token come quello dei dispositivi, in un cookie HttpOnly, e dura trenta
giorni.

Le pagine sono servite dal server (Jinja) con un po' di HTMX per le cose vive:
nessun secondo progetto da compilare, nessuna libreria presa da Internet — htmx
e le coste della mappa stanno nei file del servizio.

## API

| Metodo | Rotta | A cosa serve |
|---|---|---|
| POST | `/v1/auth/signup` | nominativo + password → token |
| POST | `/v1/auth/token` | nominativo + password → token |
| POST | `/v1/sync/push` | manda i QSO in coda e i documenti (profili, impostazioni) |
| GET | `/v1/sync/pull` | prende quello che e' cambiato: QSO e documenti |
| GET | `/v1/sync/status` | quanti QSO ci sono, a che punto e' il cursore |
| GET | `/v1/health` | per il monitoraggio, e dice cosa sa fare (`features`) |

Il token va nell'intestazione `Authorization: Bearer <token>`. La password si
tiene con Argon2; del token il server conserva solo l'impronta SHA-256.

La descrizione completa, generata dal codice, e' su `/docs` (OpenAPI).

## Provarlo sul proprio computer

```bash
cd server
python -m venv .venv
.venv/Scripts/pip install -r requirements.txt      # su Linux: .venv/bin/pip
.venv/Scripts/python -m uvicorn decolog_cloud.main:app --port 8787
```

Senza altre variabili usa un SQLite nella cartella corrente. Poi in DecoDXLog:
Impostazioni → Sync e Cloud, server `http://127.0.0.1:8787`.

## In servizio

```bash
docker compose up -d
```

Sono due container: il servizio e PostgreSQL. Le variabili che contano:

| Variabile | Predefinito | Cosa fa |
|---|---|---|
| `DECOLOG_DATABASE_URL` | SQLite locale | `postgresql+psycopg://utente:password@host/decolog` |
| `DECOLOG_ALLOW_SIGNUP` | `1` | a `0` chiude la registrazione: nessuno si fa un account da solo |
| `DECOLOG_APPROVAL` | `1` | chi si registra aspetta il via libera; a `0` entra subito, come prima |
| `DECOLOG_PUBLIC_URL` | `https://cloud.ft2.it` | l'indirizzo da fuori, per i collegamenti nell'email |
| `DECOLOG_PAGE_SIZE` | `500` | quanti QSO per pagina di pull |
| `DECOLOG_TOKEN_DAYS` | `180` | quanto dura un token |
| `DECOLOG_SMTP_HOST` | `smtp.gmail.com` | il server di posta per l'avviso |
| `DECOLOG_SMTP_PORT` | `587` | `587` STARTTLS, `465` cifrato dall'inizio |
| `DECOLOG_SMTP_USER` | — | la casella da cui parte l'avviso |
| `DECOLOG_SMTP_PASSWORD` | — | la app password di quella casella, non la password dell'account |
| `DECOLOG_NOTIFY_EMAIL` | — | a chi arriva l'avviso |
| `DECOLOG_SIGNUP_NOTIFY_MINUTES` | `10` | un avviso per indirizzo ogni tot minuti |

Senza le quattro variabili della posta l'avviso non parte: resta una riga nel
registro, la registrazione funziona lo stesso e gli account restano in attesa —
si approvano aprendo `/admin/signup/<id>/<chiave>`, che si legge dal database.
Un servizio non si pianta perche' non riesce a mandare una email.

Dietro un proxy con HTTPS: il token viaggia in chiaro, quindi **niente HTTP su
Internet**. In casa, sulla propria rete, va benissimo com'e'.

### Aggiornare

```bash
cd /srv/decolog && git pull && sudo bash server/deploy/update.sh
```

Ricopia il codice, aggiorna le dipendenze e riavvia. Poi controlla che quello
che risponde sia davvero la versione nuova: `/v1/health` dice cosa sa fare
(`"features": ["qso", "docs", "web"]`), e se manca qualcosa — o il servizio non
torna su — rimette da solo la versione di prima e lo dice. Un aggiornamento a
meta' e' peggio di nessuno.

Le tabelle nuove le crea il servizio all'avvio (`create_all`): nessuna
migrazione a mano, nemmeno su PostgreSQL.

## Prove

```bash
PYTHONPATH=. .venv/Scripts/python -m pytest tests -q
```

Settantatre prove: registrazione, token scaduto, push e pull, cursore, conflitti
con lo storico, revisione vecchia, duplicati, cancellazioni, pagine, documenti
(profili e impostazioni, con il loro storico e il cursore condiviso), i conti
del log (prefisso WPX con gli esempi di CQ, gruppi di modi, diplomi lavorati e
confermati per banda, QSL, locatori sulla mappa), la finestra del browser (le tre colonne, le
cinque schede, il tema che arriva dalle impostazioni), e due account che non si
vedono fra loro.
