# Storia di DecoLog

Le date sono quelle del lavoro, non di una pubblicazione: DecoLog cresce mentre lo si usa
in stazione.

## 0.3.0 — in lavorazione

**DecoLog Cloud: il sync fra dispositivi (Fase 3).** Il log resta il file SQLite, che
funziona anche senza rete; il Cloud è il posto dove i dispositivi si passano le modifiche.

Il servizio sta in `server/`: FastAPI e SQLAlchemy, SQLite per provarlo sul proprio
computer e PostgreSQL in servizio, Dockerfile e compose già pronti. Registrazione con
nominativo e password (tenuta con Argon2), e un token per dispositivo di cui il server
conserva solo l'impronta.

Dentro DecoLog: Impostazioni → Sync e Cloud per l'indirizzo, l'accesso e il sync
automatico; "Sincronizza adesso" anche nella barra in alto, con la coda sempre in vista.
Un giro fa prima il pull e poi il push, così le revisioni partono allineate. Le regole
sono quelle scritte in Fase 0: chi spinge dice la revisione che conosceva, **vince
l'ultima modifica** e la versione che perde resta nello storico; il pull non sovrascrive
mai una modifica locale ancora da mandare; i duplicati con un altro uuid si riconoscono
per nominativo, banda, gruppo di modi e orario vicino; le cancellazioni viaggiano come
modifiche. La password passa una volta sola: DecoLog tiene solo il token, nel portachiavi.

Provato per davvero fra due log: 40 QSO spinti dal primo e ripresi dal secondo, una
modifica che fa il giro, un conflitto risolto con la versione perdente nello storico del
server, e una cancellazione che arriva dall'altra parte.

**Rotore.** DecoLog parla con **DecoRotor** sul WebSocket (8765) e, per chi ha altro, con un
**rotctld** qualsiasi (DecoRotor stesso risponde sulla 4532). Il quadrante è quello di
DecoRotor, portato dentro DecoLog: corona graduata con le tacche ogni 2° e i numeri ogni
10°, mappa azimutale equidistante centrata sul proprio QTH (la direzione letta sulla corona
è la rotta vera, e la distanza dal centro cresce con i chilometri), cerchi di distanza, lobo
d'antenna, bersaglio tratteggiato e ago che gira dalla parte giusta. Sta in piccolo nella
colonna di destra, e con Ctrl+R si apre **il posto di comando**: la pagina "Controllo" di
DecoRotor rifatta com'e', con i suoi colori e le sue misure — testata con le spie (control
box, rotazione, client, modello, luce), quadrante sopra e mappa satellitare sotto divisi da
una maniglia, e a destra il display con l'azimut a caratteri grandi e l'indicatore CCW/CW,
le sei memorie a tasto diretto, i passi con lo STOP al centro, PARK, l'elenco delle memorie
e il puntamento a gradi con le otto direzioni. Sotto, la striscia di stato con le tre porte
del gateway. I riquadri della mappa arrivano dal gateway stesso (che fa da cache), gli spot
sono quelli del cluster di DecoLog e la barra in fondo punta per locatore, rotta breve o
lunga. Ci sono anche le altre due schede dell'originale: **DIAGNOSTICA** (i frame Prosistel che
passano sulla seriale con il loro esadecimale, l'andamento della posizione, i contatori
dell'esercizio e i tre indirizzi di rete) e **IMPOSTAZIONI** (nominativo, locatore, apertura
del lobo, finecorsa, riposo, tolleranza e lo stop se cade il collegamento), che scrivono nel
config.json del gateway con `config_set`. Memorie, finecorsa, riposo e lobo li dice il
gateway: DecoLog li legge e li rimanda, non se li inventa. Dove la rotta si sa già la si usa: **dal menu di
uno spot del cluster** (“punta il rotore su DL9ZZT, 287°”), dal nominativo che si sta
lavorando, e, se lo si accende, seguendo da solo quello che Decodium lavora. La direzione
dell'antenna si vede anche sulla mappa. La seriale resta a DecoRotor: i finecorsa sono del
control box.

**Propagazione.** Scheda nuova in basso: SFI, macchie, indice A e K, aurora, raggi X, campo
geomagnetico, rumore e vento solare, e le condizioni banda per banda di giorno e di notte
(più aurora ed E-skip in VHF), colorate. I dati arrivano dal XML di N0NBH (hamqsl.com), da
soli ogni ora o a comando. DecoLog tiene un campione all'ora e lo mette accanto ai QSO di
quel giorno: negli ultimi quattordici giorni si vede se il proprio ritmo segue davvero il
flusso solare. In testa alla mappa restano SFI e K, dove si guarda la propagazione.

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
con “metti in coda tutte quelle da ricambiare”. Da lì escono le **etichette in PDF**:
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
