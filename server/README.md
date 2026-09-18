# DecoLog Cloud

Il servizio che tiene allineato il log fra i dispositivi di una stazione: il PC
dello shack, il portatile che va in portatile, domani il telefono.

Non e' un secondo logbook. Il log vero resta il file SQLite di DecoLog, che
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
JSON. Il giorno che DecoLog impara un campo nuovo, qui non si tocca niente.

## Non solo i QSO

Un log non e' solo l'elenco dei collegamenti: chi apre DecoLog sul secondo
computer si deve ritrovare la stessa stazione. Oltre ai QSO viaggiano quindi i
**documenti**, con le stesse regole (revisione, ultima modifica che vince,
storico, stesso cursore):

* `profile` — un documento per profilo stazione: nominativo di stazione,
  operatore, locatore, radio, antenna, potenza, e quale e' il predefinito.
* `setting` / `station` — un documento solo con le impostazioni che fanno parte
  del modo di lavorare: tema e lingua, filtri salvati e colonne del log, avvisi
  e fonti del cluster, premi seguiti, invii automatici (LoTW, QSL),
  propagazione, lobo del rotore, dedup della UDP, backup, sync automatico.

Quello che **non** passa di qui, per scelta: porte, percorsi e indirizzi dei
programmi accanto — sono di *quella* macchina — e tutto cio' che sta nel
portachiavi. Password e chiavi dei servizi non arrivano mai al server.

I documenti stanno nella stessa spinta dei QSO (`docs` in `/v1/sync/push`,
`docResults` nella risposta) e nello stesso pull: un giro solo, un cursore solo.

## Il log dal browser

Oltre all'API c'e' la pagina: si entra con gli stessi nominativo e password del
programma e si vede il proprio log — tabella con ricerca mentre si scrive,
filtri per banda e modo, la pagina si allunga da sola scorrendo, scheda del
singolo QSO con tutti i campi ADIF, la pagina **Stazione** con i profili e le
impostazioni arrivate dal programma, e il tasto per **scaricare tutto in ADIF**:
il Cloud non e' una gabbia.

Da qui si guarda e si scarica; si scrive dal programma. La sessione del browser
e' un token come quello dei dispositivi, in un cookie HttpOnly, e dura trenta
giorni.

Le pagine sono servite dal server (Jinja) con un po' di HTMX per le cose vive:
nessun secondo progetto da compilare, nessuna libreria presa da Internet — htmx
sta nei file del servizio.

## API

| Metodo | Rotta | A cosa serve |
|---|---|---|
| POST | `/v1/auth/signup` | nominativo + password → token |
| POST | `/v1/auth/token` | nominativo + password → token |
| POST | `/v1/sync/push` | manda i QSO in coda e i documenti (profili, impostazioni) |
| GET | `/v1/sync/pull` | prende quello che e' cambiato: QSO e documenti |
| GET | `/v1/sync/status` | quanti QSO ci sono, a che punto e' il cursore |
| GET | `/v1/health` | per il monitoraggio |

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

Senza altre variabili usa un SQLite nella cartella corrente. Poi in DecoLog:
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
| `DECOLOG_PAGE_SIZE` | `500` | quanti QSO per pagina di pull |
| `DECOLOG_TOKEN_DAYS` | `180` | quanto dura un token |

Dietro un proxy con HTTPS: il token viaggia in chiaro, quindi **niente HTTP su
Internet**. In casa, sulla propria rete, va benissimo com'e'.

## Prove

```bash
PYTHONPATH=. .venv/Scripts/python -m pytest tests -q
```

Trentuno prove: registrazione, token scaduto, push e pull, cursore, conflitti con
lo storico, revisione vecchia, duplicati, cancellazioni, pagine, documenti
(profili e impostazioni, con il loro storico e il cursore condiviso), le pagine
del browser, e due account che non si vedono fra loro.
