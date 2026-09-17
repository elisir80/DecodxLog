# DecoLink — protocollo locale Decodium ⇄ DecoLog

Versione del protocollo: **1**. Stato: prima implementazione (DecoLog 0.1, ramo
`decolink` di Decodium 4.0).

Il protocollo UDP di WSJT-X resta il canale con cui i QSO arrivano a DecoLog: funziona
con tutti i programmi e non cambia. DecoLink aggiunge quello che UDP non può dare,
cioè l'altra direzione: DecoLog che dice a Decodium cosa c'è nel log.

## Trasporto

- TCP su `127.0.0.1`, porta **52237** (configurabile in entrambi i programmi).
  Solo loopback: DecoLog non accetta connessioni da altri indirizzi.
- DecoLog è il server, Decodium il client. Decodium si ricollega da solo ogni 5 s
  finché DecoLog non è aperto.
- Un messaggio per riga: un oggetto JSON in UTF-8 terminato da `\n`. Nessun
  messaggio contiene `\n` dentro.
- Ogni messaggio ha `"type"`. Campi sconosciuti si ignorano; tipi sconosciuti si
  ignorano. Così le versioni successive restano compatibili.

## Apertura

Appena connesso, ognuno manda `hello`:

```json
{"type":"hello","app":"Decodium","version":"1.0.638","protocol":1,"station":"IU8LMC"}
{"type":"hello","app":"DecoLog","version":"0.1.0","protocol":1,"station":"IU8LMC"}
```

Dopo il suo `hello`, DecoLog manda subito l'elenco dei QSO lavorati e lo stato
dell'FT2 Award.

## DecoLog → Decodium

### `worked` — il log, a blocchi

```json
{"type":"worked","seq":1,"final":false,"rows":[["DL2ABC","20m","FT8","20260916","JO31",1], ...]}
```

Ogni riga: `[call, banda, modo, data yyyyMMdd, locatore4, confermato]`.
- `modo` è quello che legge l'operatore: `FT2` e non `MFSK`, `SSB` e non `USB`.
- `confermato` è 1 se il QSO è confermato secondo le conferme scelte negli award
  di DecoLog (LoTW e cartolina per default), altrimenti 0.
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

## Decodium → DecoLog

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
