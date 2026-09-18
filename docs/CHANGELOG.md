# Storia di DecoLog

Le date sono quelle del lavoro, non di una pubblicazione: DecoLog cresce mentre lo si usa
in stazione.

## Non ancora pubblicato

**Tutto il log sul Cloud, non solo i QSO.** Chi si collega da un secondo computer non
deve rifare la stazione a mano: adesso viaggiano anche i **profili stazione**
(nominativo di stazione, operatore, locatore, radio, antenna, potenza, quello
predefinito) e **tutte le impostazioni** — tema, lingua, colonne e filtri salvati del
log, fonti e avvisi del cluster, premi seguiti, invii automatici (LoTW, QSL),
propagazione, rotore, dedup della UDP, backup, e anche porte, percorsi e indirizzi dei
programmi accanto. Una stazione che si ritrova uguale, non una che le somiglia.

**Anche le password dei servizi, ma chiuse.** QRZ, LoTW, Club Log, eQSL, HamQTH,
HamAlert: sul secondo computer non si riscrivono a mano. Viaggiano — sigillate qui,
con **AES-256-GCM** e una chiave che nasce dalla password del Cloud (PBKDF2-HMAC-SHA256,
200.000 giri), quella che il server conosce solo come impronta Argon2. Al server arriva
un blocco di byte che **senza quella password non si apre**: nemmeno per chi avesse il
database in mano. Nessuna crittografia scritta a mano: e' OpenSSL, quello che sta sotto a
HTTPS. Sull'altro dispositivo si entra con la stessa password e i servizi sono pronti.
La chiave non passa mai dal server: si rifa' dalla password e poi vive nel portachiavi
accanto al token; "Scollega" la butta. Si spegne dall'interruttore in Impostazioni →
Sync e Cloud, e senza OpenSSL DecoLog lo dice e non manda niente. Chi si era collegato
**prima** che la cassaforte esistesse ha la chiave mancante: nella stessa pagina compare
"Apri la cassaforte", si dice la password una volta e basta — non serve scollegarsi.

Restano fuori solo due cose, e nessuna e' una scelta di chi opera: il **promemoria di
cosa e' salvato nel portachiavi di quella macchina**, che altrove farebbe credere a
DecoLog di avere una password che non ha, e il **quaderno del sync** (nominativo
collegato, ora dell'ultimo giro). Il profilo attivo viaggia per **uuid** e non per numero
di riga, cosi' sul secondo computer si accende lo stesso profilo anche se li' ha un altro
numero.

Il meccanismo e' quello dei QSO, cosi' le regole non si sdoppiano: ogni profilo e' un
documento con la sua revisione, le impostazioni sono un documento solo (`station`) con
un'impronta SHA-256 che dice se e' cambiato davvero qualcosa; server e programma si
scambiano `docs` e `docResults` dentro la stessa spinta e lo stesso cursore dei QSO.
Vince l'ultima modifica, la versione che perde resta nello storico. Le impostazioni che
JSON non sa dire (un filtro salvato e' un QVariant di Qt) viaggiano impacchettate, senza
perdere niente. In arrivo, il tema si ridipinge subito: non si aspetta il riavvio.

**Il log dal browser e' la stessa finestra del programma.** Non una pagina web che
parla dello stesso log: barra superiore a blocchi, tre colonne di pannelli — scheda del
QSO a sinistra, log in mezzo, scheda del nominativo con FT2 Award e mappa a destra — le
sei schede in basso — le stesse del programma: Diplomi, Statistiche, Invio QSL,
Registro attivita', DX Cluster, Propagazione — e la barra di stato. Si sceglie un QSO nel log e le colonne seguono, come nel programma.

E **i colori sono quelli della stazione**: il tema arriva con le impostazioni
sincronizzate, valore per valore dal ThemeManager — Ocean Blue, Stellar Light o
Darkcodium con la sua variante d'accento e la densita' delle righe. Chi ha il log in
Darkcodium ambra lo ritrova in Darkcodium ambra anche sul telefono.

Cambia una cosa sola, ed e' voluta: da qui si guarda e si scarica, si scrive dal
programma.

**Dentro le schede c'e' il resto del log.** Ci sono le stesse schermate del
programma, rifatte dal log che sta sul server: **Statistiche** (QSO per anno, mese, ora
UTC, banda, modo, continente, e la mappa di calore banda per ora che dice quando una
banda e' aperta), **Diplomi** (DXCC, FT2, WAZ, WAS, WPX, locatori, IOTA, POTA, SOTA,
WWFF: lavorati e confermati, il conto per banda, i band slot, e l'elenco di quello che
manca), **QSL** (inviate e ricevute servizio per servizio, e le ultime conferme),
**Mappa** (i locatori lavorati sul mondo, coste comprese) e **Stazione** (profili e
impostazioni).

**Propagazione** legge la stessa fonte del programma (il XML di N0NBH) una volta
all'ora: SFI, macchie, indice A e K, aurora, MUF, le condizioni banda per banda di giorno
e di notte con i loro colori, il VHF e il resto. Se la fonte non risponde si mostra
l'ultimo dato buono con la sua ora, invece di una pagina vuota.

Nella scheda **Invio QSL** ci sono le colonne del programma — da mandare, inviate,
confermate — e la **coda delle QSL di carta**: quelle che aspettano la cartolina, quelle
gia' partite e per che via (bureau, diretta, manager), quelle tornate.

I numeri non stanno in tabelle di riepilogo: si rifanno dai QSO a ogni richiesta, con le
stesse regole del programma — `server/decolog_cloud/analytics.py` e' `src/core/Awards.cpp`
portato in Python, gruppi di modi compresi, il prefisso WPX di CQ e i cinquanta stati.
Cosi' una correzione a un QSO si vede subito da tutte e due le parti, e la pagina non puo'
dire una cosa diversa dal programma.

Provato fra due log: il primo ha spinto 40 QSO, un profilo e 39 impostazioni; il
secondo, partito vuoto, si e' ritrovato il profilo "Casa di prova" gia' predefinito e le
impostazioni alla revisione 1, senza toccare niente a mano. E tre giri di sync di fila
non ne rimandano nemmeno una.

## 0.3.0 — 18 settembre 2026

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

**Il log dal browser.** Sullo stesso servizio c'e' la pagina: si entra con gli stessi
nominativo e password, e si vede il proprio log — tabella con la ricerca mentre si scrive,
filtri per banda e modo, la pagina che si allunga scorrendo, la scheda del QSO con tutti i
campi ADIF, e il tasto per riscaricare tutto in ADIF. Da qui si guarda e si scarica: si
scrive dal programma. Pagine servite dal server (Jinja) con un po' di HTMX tenuto in casa,
i colori sono quelli di DecoLog, e la sessione e' un cookie HttpOnly che dura trenta
giorni.

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
