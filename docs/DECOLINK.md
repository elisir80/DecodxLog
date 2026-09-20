# DecoLink — protocollo locale Decodium ⇄ DecoDXLog

Versione del protocollo: **1**. Stato: prima implementazione (DecoDXLog 0.1, ramo
`decolink` di Decodium 4.0).

Il protocollo UDP di WSJT-X resta il canale con cui i QSO arrivano a DecoDXLog: funziona
con tutti i programmi e non cambia. DecoLink aggiunge quello che UDP non può dare,
cioè l'altra direzione: DecoDXLog che dice a Decodium cosa c'è nel log.

## Trasporto

- TCP su `127.0.0.1`, porta **52237** (configurabile in entrambi i programmi).
  Solo loopback: DecoDXLog non accetta connessioni da altri indirizzi.
- DecoDXLog è il server, Decodium il client. Decodium si ricollega da solo ogni 5 s
  finché DecoDXLog non è aperto.
- Un messaggio per riga: un oggetto JSON in UTF-8 terminato da `\n`. Nessun
  messaggio contiene `\n` dentro.
- Ogni messaggio ha `"type"`. Campi sconosciuti si ignorano; tipi sconosciuti si
  ignorano. Così le versioni successive restano compatibili.

## Apertura

Appena connesso, ognuno manda `hello`:

```json
{"type":"hello","app":"Decodium","version":"1.0.638","protocol":1,"station":"IU8LMC"}
{"type":"hello","app":"DecoLog","product":"DecoDXLog","version":"0.9.1","protocol":1,"station":"IU8LMC"}
```

**`app` e' il nome del protocollo, non quello del programma.** Dalla prima versione
questo lato si presenta come `DecoLog`, e i Decodium gia' installati chiudono la
connessione se leggono un nome diverso: quando il programma e' stato rinominato in
DecoDXLog e il saluto e' cambiato con lui, il collegamento cadeva ogni cinque secondi.
`app` resta `DecoLog` per sempre; il nome vero del programma viaggia in **`product`**,
che chi non lo conosce ignora.

Dopo il suo `hello`, DecoDXLog manda subito l'elenco dei QSO lavorati e lo stato
dell'FT2 Award.

## DecoDXLog → Decodium

### `worked` — il log, a blocchi

```json
{"type":"worked","seq":1,"final":false,"rows":[["DL2ABC","20m","FT8","20260916","JO31",1], ...]}
```

Ogni riga: `[call, banda, modo, data yyyyMMdd, locatore4, confermato]`.
- `modo` è quello che legge l'operatore: `FT2` e non `MFSK`, `SSB` e non `USB`.
- `confermato` è 1 se il QSO è confermato secondo le conferme scelte negli award
  di DecoDXLog (LoTW e cartolina per default), altrimenti 0.
- Blocchi da 2000 righe; `final: true` sull'ultimo (anche se vuoto). Chi riceve
  unisce le righe ai propri insiemi "lavorato": DecoLink aggiunge, non toglie.

### `qso` — un QSO appena scritto nel log

```json
{"type":"qso","row":["9A3XY","20m","FT2","20260917","JN75",0],"status":"logged","uuid":"7f3a…","source":"udp_decodium","app":"Decodium FT2 1.0.637"}
```

`status`: `logged` (scritto), `duplicate` (c'era già), `error` (non scritto; c'è
`message`). Arriva per ogni QSO, qualunque ne sia la provenienza (UDP, scheda a mano,
import di un file). Per i QSO arrivati da Decodium è la conferma di scrittura.

### `award` — stato dell'FT2 Award

```json
{"type":"award","ft2":{"qsos":42,"dxccWorked":31,"dxccConfirmed":18,"gridsWorked":64,"gridsConfirmed":30},"dxcc":{"worked":112,"confirmed":87}}
```

Mandato dopo `hello` e ogni volta che il log cambia.

### `status` — risposta a `query`

```json
{"type":"status","id":7,"results":[{"call":"9A1AA","dxcc":497,"entity":"Croatia","workedCall":false,"workedCallBand":false,"workedDxcc":true,"workedDxccBand":true,"workedDxccFt2":false,"confirmedDxcc":false}]}
```

### `spot` — uno spot del cluster di DecoDXLog

```json
{"type":"spot","call":"3Y0J","freqKhz":14025.1,"dialKhz":14025.1,"audioHz":0,"band":"20m","mode":"CW","spotter":"DL1ABC-#","comment":"CW 22 dB 25 WPM CQ","time":"2026-09-17T12:39:00Z","source":"rbn","entity":"Bouvet","dxcc":24,"status":"NEW DXCC","statusBits":3,"count":2,"alert":true}
```

Gli spot che passano i filtri del cluster di DecoDXLog, piu' quelli che fanno scattare
una regola d'avviso (`alert`). Gia' confrontati col log: `status` e' l'etichetta
principale (`NEW DXCC`, `NEW BAND`, `NEW MODE`, `NEW SLOT`, `WORKED`, `NEW CALL` o
vuota), `statusBits` i bit completi (1 nuovo DXCC, 2 nuova banda, 4 nuovo modo, 8
nuovo slot, 16 nuovo nominativo, 32 gia' lavorato sulla banda, 64 entita' non
confermata, 128 utente LoTW). Per FT8/FT4/FT2 `dialKhz` e' la frequenza di chiamata e
`audioHz` lo spostamento audio dello spot. `source`: `cluster`, `rbn`, `hamalert`,
`pota`.

### `tune` — sintonizzare su uno spot

```json
{"type":"tune","call":"FT4TA","freqKhz":10137.5,"dialKhz":10136,"audioHz":1500,"mode":"FT8","grid":""}
```

L'operatore ha fatto doppio clic su uno spot in DecoDXLog. Decodium porta la radio su
`dialKhz`, passa al modo se lo conosce, prepara `call` come DX e mette la frequenza
audio su `audioHz` se diversa da 0. Non trasmette mai da solo.

## Decodium → DecoDXLog

### `query` — che cosa sa il log di questi nominativi

```json
{"type":"query","id":7,"band":"20m","mode":"FT2","calls":["9A1AA","JA1ZZZ"]}
```

Al massimo 200 nominativi per richiesta. `band` e `mode` possono mancare.

### `ping`

```json
{"type":"ping","id":3}
```

Risposta `{"type":"pong","id":3}`. Decodium lo manda ogni 30 s per accorgersi di una
connessione rimasta appesa.
