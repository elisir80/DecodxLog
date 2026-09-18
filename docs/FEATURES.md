# DecoLog 0.2.0 — funzioni / features

Ogni voce è in italiano e in inglese: *italiano* / *English*.
Quello che manca è in fondo.

## 1. QSO da Decodium e WSJT-X / QSO capture

- **Ricezione UDP dei QSO** da Decodium, WSJT-X, JTDX e compatibili, porta configurabile,
  anche in multicast. / **UDP capture of logged QSOs** from Decodium, WSJT-X, JTDX and
  compatible programs, configurable port, multicast supported.
- **LoggedADIF preferito a QSOLogged**: il messaggio completo non perde campi; se arrivano
  entrambi si usa uno solo. / **LoggedADIF preferred over QSOLogged**: the full message
  loses no field; if both arrive only one QSO is written.
- **FT2 di prima classe**: `MODE=MFSK`, `SUBMODE=FT2`, e "FT2" ovunque nell'interfaccia. /
  **FT2 as a first-class mode**: `MODE=MFSK`, `SUBMODE=FT2`, shown as "FT2" everywhere.
- **Profilo stazione dal nominativo** del QSO, altrimenti quello attivo. / **Station profile
  from the QSO callsign**, otherwise the active one.
- **Entità DXCC aggiunta al volo** (DXCC, paese, zone, continente). / **DXCC entity filled
  on the fly** (DXCC, country, zones, continent).
- **Finestra dei duplicati** configurabile, diversa per QSO digitali e a mano. / **Duplicate
  window**, configurable, different for digital and manual QSOs.
- **Il nominativo che Decodium sta lavorando** segue nella scheda a destra. / **The DX call
  Decodium is working** follows in the call info panel.
- **Stato del client**: chi è collegato, frequenza, modo, TX, e l'avviso se tace. / **Client
  state**: who is connected, frequency, mode, TX, and a warning when it goes quiet.

## 2. Il log / The log

- **SQLite in modalità WAL**, nomi di colonna ADIF, un file solo, nessun server. / **SQLite
  in WAL mode**, ADIF column names, a single file, no server.
- **Import/export ADIF senza perdite**: i campi senza colonna restano in `adif_extra` e
  tornano fuori identici. / **Lossless ADIF import/export**: fields without a column live in
  `adif_extra` and come back out unchanged.
- **Storico delle revisioni**: ogni modifica è una nuova revisione, le precedenti si
  rileggono e si ripristinano. / **Revision history**: every edit is a new revision, earlier
  ones can be read and restored.
- **Cancellazione morbida**: il QSO resta nello storico e si recupera. / **Soft delete**: the
  QSO stays in history and can be recovered.
- **Pronto per il sync**: `uuid`, `revision`, `dirty` su ogni QSO. / **Ready for sync**:
  `uuid`, `revision`, `dirty` on every QSO.
- **Stato QSL per servizio** (LoTW, QRZ, Club Log, eQSL, cartolina) in una tabella a parte. /
  **QSL state per service** (LoTW, QRZ, Club Log, eQSL, card) in its own table.
- **Etichette sui QSO** (`APP_DECOLOG_TAGS`): attivazione, contest, evento, portatile. /
  **Tags on QSOs** (`APP_DECOLOG_TAGS`): activation, contest, event, portable.
- **Schema versionato con migrazione** (v2), senza toccare i QSO. / **Versioned schema with
  migration** (v2), leaving the QSOs alone.

## 3. Logbook

- **Tabella del log** con colonne a scelta, nascondibili, e QSL in colonna (L Q C E). /
  **Log table** with the columns you want, hideable, and QSL shown as L Q C E.
- **Finestra separata ("Stacca")** per un secondo monitor. / **Separate window ("Pop")** for
  a second screen.
- **Filtri**: testo, banda, modo, mese, intervallo di date, entità DXCC, stato QSL, profilo
  stazione, etichetta. / **Filters**: text, band, mode, month, date range, DXCC entity, QSL
  state, station profile, tag.
- **Filtri salvati** con un nome, da riapplicare. / **Saved filters** you can name and apply
  again.
- **Azioni sulle righe mostrate**: etichetta di gruppo, togli etichetta, export ADIF. /
  **Actions on the rows shown**: tag them all, remove a tag, export to ADIF.
- **Menu della riga**: apri, filtra per nominativo, filtra per entità, etichetta. / **Row
  menu**: open, filter by call, filter by entity, tag.
- **Tastiera**: frecce, Invio per aprire, Ctrl+N nuovo QSO, Ctrl+F cerca, Ctrl+I import,
  Ctrl+E export, Ctrl+K cluster, Ctrl+T attivazione. / **Keyboard**: arrows, Enter to open,
  Ctrl+N new QSO, Ctrl+F search, Ctrl+I import, Ctrl+E export, Ctrl+K cluster, Ctrl+T
  activation.

## 4. Nuovo QSO / New QSO

- **Pannello rapido** e **scheda completa** (Ctrl+N) con tutti i campi ADIF utili. / **Quick
  panel** and **full form** (Ctrl+N) with every useful ADIF field.
- **Ora UTC con un clic**, banda dalla frequenza, RST predefiniti. / **UTC time with one
  click**, band from frequency, default RST.
- **Il callbook riempie solo i campi vuoti** (nome, QTH, locatore). / **The callbook fills
  only empty fields** (name, QTH, grid).
- **Quello che il log sa del nominativo** mentre lo scrivi: già lavorato, nuovo DXCC, nuovo
  su questa banda. / **What the log knows about the call** as you type: worked before, new
  DXCC, new on this band.
- **Etichette** e, nei contest, **numero inviato e ricevuto**. / **Tags** and, in contests,
  **serial sent and received**.
- **Registra e continua** per una serie di QSO. / **Log & keep** for a run of QSOs.

## 5. Scheda del QSO / QSO detail

- **Tutti i campi ADIF** divisi in Generale, Posizione, QSL, ADIF extra, Storico. / **All
  ADIF fields** split into General, Location, QSL, ADIF extra, History.
- **Campi liberi**: si aggiungono e restano nell'export. / **Free fields**: you can add them
  and they survive the export.
- **QSL per servizio** con inviato, ricevuto e date. / **QSL per service** with sent,
  received and dates.
- **Effetto sui diplomi**: questo QSO è un nuovo DXCC in FT2? / **Award impact**: is this QSO
  a new DXCC on FT2?
- **Distanza e azimut** dal locatore. / **Distance and bearing** from the grid.
- **Export del singolo QSO**, ripristino di una revisione, cancellazione morbida. / **Export
  of the single QSO**, restore of a revision, soft delete.

## 6. Profili stazione / Station profiles

- **Più stazioni** (casa, portatile, evento) con nominativo, operatore, locatore, zone,
  radio, antenna, potenza, station location di LoTW. / **Several stations** (home, portable,
  event) with callsign, operator, grid, zones, rig, antenna, power, LoTW station location.
- **Profilo predefinito e profilo attivo**, duplicazione, conteggio dei QSO. / **Default and
  active profile**, duplication, QSO count.
- **Primo profilo creato da Decodium** al primo avvio. / **First profile created from
  Decodium** on the first run.

## 7. Entità DXCC / DXCC entities

- **cty.csv di AD1C incluso**, aggiornabile dalle impostazioni. / **AD1C cty.csv bundled**,
  updatable from the settings.
- **Completa i DXCC mancanti** nei QSO importati, ognuno come nuova revisione. / **Fill
  missing DXCC** on imported QSOs, each as a new revision.

## 8. Scheda del nominativo / Call info

- **Già lavorato**: quante volte, su quali bande e modi, ultimi cinque QSO. / **Worked
  before**: how many times, on which bands and modes, last five QSOs.
- **Nuovo DXCC / nuovo su banda** mentre Decodium lavora. / **New DXCC / new on band** while
  Decodium is working.
- **Distanza, azimut, ora locale approssimata**. / **Distance, bearing, approximate local
  time**.
- **Callbook QRZ.com (XML) o HamQTH**: nome, QTH, locatore, zone, foto, utente LoTW/eQSL,
  risultati tenuti un giorno. / **QRZ.com (XML) or HamQTH callbook**: name, QTH, grid, zones,
  photo, LoTW/eQSL user, results cached for a day.
- **Stato QSL dell'ultimo QSO**. / **QSL state of the last QSO**.

## 9. Diplomi / Awards

- **Calcolati dal log**: DXCC, FT2 Award, WAZ, WAS, WPX, locatori, IOTA, POTA, SOTA, WWFF. /
  **Computed from the log**: DXCC, FT2 Award, WAZ, WAS, WPX, grids, IOTA, POTA, SOTA, WWFF.
- **Tabella per banda** (● confermato, ○ lavorato) con totali per banda e **band slot**. /
  **Per-band table** (● confirmed, ○ worked) with per-band totals and **band slots**.
- **Quello che manca**: entità, zone e stati mai lavorati. / **What is missing**: entities,
  zones and states never worked.
- **Mappa dei locatori** lavorati e confermati. / **Grid map** of worked and confirmed
  squares.
- **Filtri**: banda, gruppo di modi, profilo stazione, etichetta; conferme accettate a scelta
  (LoTW, cartolina, eQSL). / **Filters**: band, mode group, station profile, tag; which
  confirmations count (LoTW, card, eQSL).
- **Pannello FT2 Award** sempre in vista. / **FT2 Award panel** always in sight.

## 10. QSL

- **Conferme LoTW scaricate** da `lotwreport.adi`, solo le nuove dall'ultimo sync, a mano o
  ogni 6/12/24 ore. / **LoTW confirmations downloaded** from `lotwreport.adi`, only the new
  ones, manually or every 6/12/24 hours.
- **Abbinamento** per nominativo, banda, gruppo di modi e ora entro mezz'ora, come fa LoTW;
  i dettagli riempiono solo i campi vuoti. / **Matching** by call, band, mode group and time
  within 30 minutes, as LoTW does; details fill only empty fields.
- **Invio a LoTW** facendo firmare un ADIF temporaneo al TQSL installato. / **Upload to
  LoTW** by having the installed TQSL sign a temporary ADIF.
- **Invio a QRZ Logbook** (chiave API) e **eQSL** (utente e password). / **Upload to QRZ
  Logbook** (API key) and **eQSL** (user and password).
- **Invio a Club Log**: il QSO appena registrato parte da solo, l'arretrato parte come un
  unico ADIF; servono email, password, nominativo e chiave API. / **Upload to Club Log**: a
  QSO just logged leaves on its own, a backlog leaves as one ADIF; it needs email, password,
  callsign and an API key.
- **A mano o automatico** dopo ogni QSO, con coda e conteggi per servizio. / **Manual or
  automatic** after each QSO, with a queue and per-service counters.
- **Duplicato = inviato**, rifiuto scritto sul QSO con il motivo. / **A duplicate counts as
  sent**, a rejection is written on the QSO with its reason.

## 10b. QSL di carta / Paper QSL

- **Coda** delle cartacee: da mandare, mandate, ricevute, e quante aspettano risposta. /
  **Queue** of paper cards: to send, sent, received, and how many await an answer.
- **In coda** dal menu della riga del log (bureau o diretta) o tutte insieme quelle da
  ricambiare. / **Into the queue** from the log row menu (bureau or direct), or every card
  waiting for an answer at once.
- **Etichette in PDF**: una per corrispondente con fino a sei QSO, quattro fogli in
  commercio, segni di taglio a scelta, senza stampante di mezzo. / **PDF labels**: one per
  correspondent with up to six QSOs, four off-the-shelf sheets, optional cutting guides, no
  printer in the way.
- **La via** (bureau, diretta, elettronica) resta sul QSO e torna nell'export come
  `QSL_SENT_VIA`. / **The route** (bureau, direct, electronic) stays on the QSO and comes
  back in the export as `QSL_SENT_VIA`.

## 10c. Contest / Contest

- **Finestra da tastiera** (Ctrl+Shift+T): nominativo, Invio registra, Esc pulisce, la barra
  passa al rapporto. / **Keyboard window** (Ctrl+Shift+T): callsign, Enter logs, Esc clears,
  space moves to the report.
- **Doppio in evidenza** mentre si scrive: stesso nominativo, stessa banda, stesso modo
  dentro la sessione. / **Dupe shown while typing**: same call, same band, same mode inside
  the session.
- **Ritmo**: QSO degli ultimi dieci minuti e dell'ultima ora, QSO/h, DXCC e locatori
  entrati. / **Rate**: QSOs in the last ten minutes and the last hour, QSOs/h, DXCC and
  grids worked.
- **Numero progressivo** dalla sessione, scambio ricevuto in un campo solo. / **Serial
  number** from the session, received exchange in one field.
- **Export Cabrillo 3.0**: testata con categorie, locatore, punteggio e soapbox, righe QSO a
  colonne fisse, frequenze in kHz (numero di banda dai 6 metri in su), modi CW/PH/RY/DG/FM. /
  **Cabrillo 3.0 export**: header with categories, grid, score and soapbox, fixed-column QSO
  lines, frequencies in kHz (band number from 6 m up), modes CW/PH/RY/DG/FM.

## 10d. Propagazione / Propagation

- **Numeri del Sole**: SFI, macchie, indice A e K, aurora, raggi X, campo geomagnetico,
  rumore, vento solare. / **Solar numbers**: SFI, sunspots, A and K index, aurora, X-ray,
  geomagnetic field, noise, solar wind.
- **Condizioni banda** di giorno e di notte, più aurora ed E-skip in VHF, colorate. / **Band
  conditions** for day and night, plus aurora and E-skip on VHF, colour-coded.
- **Da sole ogni ora** o a comando, dal XML di N0NBH (hamqsl.com). / **By themselves every
  hour** or on demand, from N0NBH's XML (hamqsl.com).
- **Storico accanto ai tuoi QSO**: ultimi quattordici giorni, barra dei QSO e flusso medio
  del giorno. / **History next to your QSOs**: last fourteen days, bar of QSOs and the day's
  average flux.
- **SFI e K in testa alla mappa**, dove si guarda la propagazione. / **SFI and K on the map
  header**, where propagation is looked at.

## 10e. Rotore / Rotor

- **DecoRotor sul WebSocket** (8765) o un **rotctld** qualsiasi (4532): DecoLog non tocca la
  seriale. / **DecoRotor over WebSocket** (8765) or any **rotctld** (4532): DecoLog never
  touches the serial port.
- **Rosa** con lobo d'antenna, gradi, bersaglio, passi da 1° e 10°, STOP e park; si punta
  cliccando. / **Compass** with the antenna lobe, degrees, target, 1° and 10° steps, STOP and
  park; click to point.
- **Dal cluster**: il menu di uno spot punta il rotore sui gradi già calcolati. / **From the
  cluster**: a spot's menu points the rotor at the bearing already computed.
- **Sul DX** che si sta lavorando, a mano o seguendolo da solo. / **On the DX** being worked,
  by hand or following it by itself.
- **Sulla mappa** si vede dove guarda l'antenna. / **On the map** you see where the antenna
  is pointing.

## 11. DX cluster

- **Fonti in un elenco solo**: nodi telnet (DX Spider, CC Cluster), Reverse Beacon Network
  (CW/RTTY e FT8/FT4), HamAlert, attivazioni POTA. / **All sources in one list**: telnet
  nodes (DX Spider, CC Cluster), Reverse Beacon Network (CW/RTTY and FT8/FT4), HamAlert,
  POTA activations.
- **Login automatico** col nominativo del profilo (come `CALL-2`, per non chiudere la
  sessione di Decodium), riconnessione da sola. / **Automatic login** with the profile
  callsign (as `CALL-2`, so Decodium keeps its own session), self reconnection.
- **Ogni spot confrontato col log**: NUOVO DXCC, nuova banda, nuovo modo, nuovo slot, già
  lavorato, entità non confermata, utente LoTW. / **Every spot compared with the log**: NEW
  DXCC, new band, new mode, new slot, worked, unconfirmed entity, LoTW user.
- **Distanza e azimut**, referenze POTA/SOTA/WWFF/IOTA lette dal commento, SNR degli
  skimmer, spot dello stesso DX raggruppati. / **Distance and bearing**, POTA/SOTA/WWFF/IOTA
  references read from the comment, skimmer SNR, spots of the same DX grouped.
- **Filtri**: banda, modo, stato, continente del DX e dello spotter, fonte, nominativi con
  jolly, testo, SNR minimo, età, solo attivazioni, solo utenti LoTW, niente skimmer, banda di
  Decodium; salvabili con un nome. / **Filters**: band, mode, status, DX and spotter
  continent, source, wildcard calls, text, minimum SNR, age, activations only, LoTW users
  only, no skimmers, Decodium's band; saveable by name.
- **Regole d'avviso** con annuncio vocale (voci di sistema, italiano o inglese, alfabeto
  fonetico), riga evidenziata nel registro e invio a Decodium. / **Alert rules** with voice
  announcements (system voices, Italian or English, phonetic alphabet), a highlighted line in
  the log and a message to Decodium.
- **Doppio clic: Decodium si sintonizza** (frequenza di chiamata, modo, DX pronto). /
  **Double-click tunes Decodium** (dial frequency, mode, DX ready).
- **Console** per i comandi al nodo e per pubblicare uno spot. / **Console** for node
  commands and for posting a spot.
- **Elenco utenti LoTW** dell'ARRL, aggiornato una volta alla settimana. / **ARRL LoTW user
  list**, refreshed once a week.

## 12. Attivazioni e contest / Activations and contests

- **Sessione POTA, SOTA, WWFF, IOTA, contest o libera**. / **POTA, SOTA, WWFF, IOTA, contest
  or free session**.
- **Campi dell'attivatore su ogni QSO**: `MY_SIG`, `MY_SIG_INFO`, `MY_POTA_REF`,
  `MY_WWFF_REF`, `MY_SOTA_REF`, `MY_IOTA`, più locatore del posto ed etichetta. / **Activator
  fields on every QSO**: same fields, plus the grid of the place and a tag.
- **Numero progressivo** inviato (`STX`) e ricevuto (`SRX`). / **Serial number** sent (`STX`)
  and received (`SRX`).
- **Duplicati dentro la sessione**, a qualunque ora. / **Duplicates inside the session**, at
  any hour.
- **Conteggi**: QSO, nominativi diversi, durata, per banda e modo, e quanto manca per
  validare (10 POTA, 4 SOTA). / **Counters**: QSOs, different calls, duration, per band and
  mode, and how many QSOs are still needed (10 POTA, 4 SOTA).
- **Export ADIF della sessione** con il nome che POTA si aspetta. / **ADIF export of the
  session** with the file name POTA expects.
- **La sessione sta nel log**: sopravvive alla chiusura del programma. / **The session lives
  in the log**: it survives a restart.

## 13. DecoLink (Decodium ⇄ DecoLog)

- **Il log dentro Decodium**: nominativi lavorati e confermati, anche quelli che non sono nel
  suo ADIF. / **The log inside Decodium**: worked and confirmed calls, including those not in
  its own ADIF.
- **Conferma di ogni QSO scritto**, con messaggio nella barra di stato di Decodium. /
  **Confirmation of every QSO written**, with a message in Decodium's status bar.
- **Stato dell'FT2 Award** in Decodium ("LOG FT2 x/y"). / **FT2 Award state** in Decodium
  ("LOG FT2 x/y").
- **Spot del cluster** dentro la lista e la cascata di Decodium, marcati. / **Cluster spots**
  inside Decodium's list and waterfall, marked.
- **Sintonia su uno spot** chiesta da DecoLog; non trasmette mai. / **Tuning on a spot**
  asked by DecoLog; it never transmits.
- **Solo 127.0.0.1**, JSON su TCP, protocollo in `docs/DECOLINK.md`. / **Localhost only**,
  JSON over TCP, protocol in `docs/DECOLINK.md`.

## 14. Copie di sicurezza / Backup

- **Copia notturna** con `VACUUM INTO`, anche mentre DecoLog scrive. / **Nightly backup**
  with `VACUUM INTO`, even while DecoLog is logging.
- **Copie a rotazione**, cartella e ora a scelta, e copia a richiesta. / **Rotating copies**,
  folder and time of your choice, and a backup on demand.
- **Se il PC era spento**, la copia si fa appena DecoLog è aperto. / **If the PC was off**,
  the copy is made as soon as DecoLog opens.

## 15. Credenziali / Credentials

- **Portachiavi di sistema** (Gestione credenziali di Windows, Portachiavi di macOS, Secret
  Service su Linux) per QRZ, QRZ Logbook, LoTW, Club Log, eQSL, HamQTH, HamAlert, Cloud. /
  **System keystore** for the same services.
- **Nel file delle impostazioni solo il nome utente**; senza portachiavi non si salva niente
  in chiaro. / **Only the user name in the settings file**; with no keystore nothing is
  stored in the clear.
- **Gli errori di rete non mostrano mai l'URL con la password**. / **Network errors never
  show the URL with the password**.

## 16. Aspetto e lingua / Look and language

- **Tre temi** (Ocean Blue, Stellar Light, Darkcodium), variante d'accento, densità, colori
  personalizzati: gli stessi di Decodium. / **Three themes** (Ocean Blue, Stellar Light,
  Darkcodium), accent variant, density, custom colours: the same as Decodium.
- **Interfaccia in italiano o in inglese**, o come il sistema. / **Interface in Italian or
  English**, or like the system.
- **Pannelli ridimensionabili**, logbook e cluster in finestre proprie. / **Resizable
  panels**, logbook and cluster in their own windows.
- **Icona propria** in tutte le misure, con i colori del tema. / **Its own icon** in every
  size, in the theme colours.

## 17. Mappa e statistiche / Map and statistics

- **Mappa** con le coste del mondo (Natural Earth, pubblico dominio, dentro l'eseguibile),
  senza scaricare niente. / **Map** with the world coastlines (Natural Earth, public domain,
  inside the executable), nothing downloaded.
- **Linea grigia** calcolata dalla posizione del Sole, aggiornata da sola. / **Grey line**
  computed from the Sun's position, refreshed by itself.
- **Locatori lavorati**, **spot del cluster** colorati per stato, la stazione e il **cerchio
  massimo** verso il nominativo scelto; livelli accendibili e spegnibili. / **Worked grids**,
  **cluster spots** coloured by status, your station and the **great circle** to the selected
  call; layers you can switch on and off.
- **Statistiche** in una finestra propria: totali, QSO per anno, per mese, per ora UTC e per
  banda, modi e continenti, e la **mappa di calore banda per ora**, che dice quando una banda
  e' aperta. Filtri per modo e per anno. / **Statistics** in their own window: totals, QSOs
  per year, month, UTC hour and band, modes and continents, and the **band-by-hour heat
  map**, which shows when a band is open. Filters by mode and by year.
- **Registro attività**: tutto quello che il programma fa, con i colori della gravità. /
  **Activity log**: everything the program does, coloured by severity.

## 18. Strumenti e confezione / Tools and packaging

- **`decolog_udpsend`**: finge di essere Decodium e manda un QSO di prova. / **`decolog_udpsend`**:
  pretends to be Decodium and sends a test QSO.
- **`decolog_clusterprobe`**: prova una fonte di spot da riga di comando. /
  **`decolog_clusterprobe`**: tries a spot source from the command line.
- **Opzioni**: `--db`, `--port`, `--import`, `--theme`, `--show`, `--grab`, `--spots`. /
  **Options**: the same.
- **`scripts/deploy.sh`**: cartella autonoma e `DecoLog-<versione>-win64.zip`. /
  **`scripts/deploy.sh`**: a stand-alone folder and `DecoLog-<version>-win64.zip`.
- **CI su GitHub** (Windows MSYS2 e Linux) con l'artefatto pronto. / **GitHub CI** (Windows
  MSYS2 and Linux) with the artifact ready.
- **15 gruppi di test** automatici. / **15 automated test suites**.

## Non ancora / Not yet

- Club Log. / Club Log.
- DecoLog Cloud e sync fra dispositivi. / DecoLog Cloud and sync between devices.
