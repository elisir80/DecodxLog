# Storia di DecoLog

Le date sono quelle del lavoro, non di una pubblicazione: DecoLog cresce mentre lo si usa
in stazione.

## 0.3.0 — in lavorazione

**Contest.** Una finestra fatta per la tastiera (Ctrl+Shift+T): si scrive il nominativo,
Invio registra, Esc pulisce, la barra passa al rapporto. Mentre si scrive si vede se è un
doppio (in questa sessione, su questa banda, in questo modo), che ritmo si tiene (QSO
degli ultimi dieci minuti e dell'ultima ora, e i QSO/h che ne verrebbero), quanti DXCC e
locatori sono entrati, e gli ultimi QSO fatti. Il numero progressivo lo mette la sessione.

**Cabrillo.** Il log del contest esce come lo vuole chi lo riceve: testata 3.0 con
categorie, locatore, punteggio dichiarato e soapbox, e una riga per QSO a colonne fisse.
Le frequenze in kHz, dai 6 metri in su il numero di banda; i modi come li vuole Cabrillo
(CW, PH, RY, DG, FM) e FT2 come digitale.

**QSL di carta.** Una finestra propria con la coda: da mandare, mandate, ricevute, e
quante aspettano risposta. Un QSO ci finisce dal menu della riga nel log o tutto insieme
con “metti in coda tutte quelle da ricambiare”. Da liì escono le **etichette in PDF**:
una per corrispondente, con dentro fino a sei QSO, perché una cartolina sola risponde a
tutti i collegamenti fatti con quella stazione. Quattro fogli in commercio (Avery L7160,
L7163, L7165 e 70 × 36 mm), segni di taglio a scelta, nessuna stampante di mezzo: il PDF
si stampa quando si vuole. La via (bureau, diretta, elettronica) resta sul QSO e torna
nell'export come `QSL_SENT_VIA`. Schema del database alla versione 3, con migrazione.

**Club Log.** Invio dei QSO a Club Log: quello appena registrato parte da solo via
realtime.php, l'arretrato parte come un unico file ADIF. Servono l'email e la password
dell'account, il nominativo del profilo stazione e una chiave API (gratuita, si chiede su
clublog.org/need_api.php) che si mette in Impostazioni → Servizi QSL. Una chiave o una
password sbagliata si legge così com'è scritta da Club Log e non viene ritentata
all'infinito; un duplicato conta come inviato.

**Statistiche** in una finestra propria: totali (QSO, nominativi, entita', locatori, primo e
ultimo QSO, giorno e ora migliori), QSO per anno, per mese, per ora UTC e per banda, modi e
continenti, e la mappa di calore banda per ora UTC — quella che dice a colpo d'occhio quando
una banda e' aperta. Filtri per modo e per anno.

**Mappa** rifatta: coste del mondo (Natural Earth, pubblico dominio, 29 kB dentro
l'eseguibile), linea grigia calcolata dalla posizione del Sole, locatori lavorati, spot del
cluster colorati per stato, la stazione e il cerchio massimo verso il nominativo scelto.
Livelli accendibili e spegnibili.

## 0.2.0 — 18 settembre 2026

**Interfaccia in italiano.** Tutte le stringhe tradotte (`translations/decolog_it.ts`), la
lingua segue il sistema oppure si sceglie in Impostazioni → Generale.

**QSL.**

- Conferme LoTW scaricate da `lotwreport.adi`, solo quelle nuove dall'ultimo sync, a mano o
  ogni 6/12/24 ore; abbinamento per nominativo, banda, gruppo di modi e ora entro mezz'ora.
  I dettagli di LoTW riempiono solo i campi vuoti, i nuovi DXCC confermati finiscono nel
  registro attività.
- Invio: LoTW facendo firmare un ADIF temporaneo al TQSL installato, QRZ Logbook con la
  chiave API, eQSL con utente e password. A mano o automatico dopo ogni QSO; un duplicato
  conta come inviato, un rifiuto resta scritto sul QSO con il motivo.

**DX cluster.** Nodi telnet (DX Spider, CC Cluster), Reverse Beacon Network, HamAlert e
attivazioni POTA in un elenco solo, con gli spot confrontati col log (nuovo DXCC, nuova
banda, nuovo modo, nuovo slot, già lavorato, entità non confermata, utente LoTW). Filtri
completi e salvabili, regole d'avviso con annuncio vocale, invio degli spot a Decodium e
doppio clic per sintonizzarlo. Console per i comandi al nodo e per mandare spot.

**Log.** Etichette sui QSO (`APP_DECOLOG_TAGS`), filtri per entità DXCC, stato QSL, profilo
stazione, etichetta e intervallo di date, azioni sulle righe mostrate (etichetta di gruppo,
export ADIF). Schema del database alla versione 2, con migrazione.

**Award.** Totali per banda e band slot, elenco di quello che manca (DXCC, FT2, WAZ, WAS),
mappa dei locatori lavorati e confermati, filtri per profilo stazione ed etichetta.

**Attivazioni e contest.** Sessione POTA/SOTA/WWFF/IOTA o contest: campi dell'attivatore su
ogni QSO, locatore del posto, etichetta, numero progressivo, duplicati contati dentro la
sessione, conteggi e export ADIF con il nome che POTA si aspetta.

**Confezione.** Icona propria (`resources/make_icon.py`) nell'eseguibile e nelle finestre,
versione nelle proprietà del file, `scripts/deploy.sh` che prepara anche
`DecoLog-<versione>-win64.zip` con LEGGIMI e strumenti di prova.

**Correzioni.** Gli errori di rete non riportano più l'URL con la password; la freccia degli
elenchi a discesa apre la tendina anche nelle caselle in cui si può scrivere.

## 0.1.0 — 17 settembre 2026

Prima versione: ricezione dei QSO da Decodium e WSJT-X via UDP, log SQLite con nomi ADIF e
storico delle revisioni, profili stazione, import/export ADIF senza perdite, logbook con
filtri, scheda del nominativo, entità DXCC dal `cty.csv` di AD1C, credenziali nel
portachiavi di sistema, callbook QRZ.com e HamQTH, award calcolati dal log, copie di
sicurezza notturne, DecoLink verso Decodium.
