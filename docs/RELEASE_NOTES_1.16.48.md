# DecoDXLog 1.16.48 / Note di rilascio

## Italiano

Questa versione raccoglie gli aggiornamenti dalla 1.16.45 alla 1.16.48.

- HamAward e N1MM: migliorati i messaggi UDP, le traduzioni e la chiusura del
  programma, per evitare invii incompleti e attese inutili all'uscita.
- Registro e diplomi: le conferme LoTW, cartoline ed eQSL hanno indicatori piu'
  chiari; il pannello rotore puo' associare un'antenna e il suo scostamento a
  ogni banda.
- CAT e CW: corretto il passaggio delle macro CW via CAT Yaesu e resi piu'
  visibili i motivi per cui una radio rifiuta un comando.
- CAT avanzato: per i modelli Hamlib che lo dichiarano, le Impostazioni offrono
  bit dati, bit di stop, parita', handshake, stati DTR/RTS e indirizzo CI-V.
  I valori vengono conservati e passati a `rigctld`; le opzioni non supportate
  dalla radio non vengono mostrate.
- Aggiornamenti: DecoDXLog cerca prima le release firmate di
  `elisir80/DecodxLog` compatibili con sistema e architettura, poi usa
  `iu8lmc/DecoDXLog` come fallback. Windows seleziona solo setup `.exe`, macOS
  solo `.dmg`, Linux solo `.AppImage`; ogni pacchetto del fork e' verificato
  con una chiave dedicata prima dell'installazione.
- Packaging: i workflow producono manifest firmati separati per Windows x64,
  macOS Apple Silicon/Intel e Linux x86_64/aarch64. Il setup Windows viene
  pubblicato soltanto dopo la prova reale dell'artefatto MinGW.

## English

This release collects the changes from 1.16.45 through 1.16.48.

- HamAward and N1MM: improved UDP messages, translations and application
  shutdown, avoiding incomplete sends and unnecessary delay when closing.
- Logbook and awards: LoTW, paper-card and eQSL confirmations now have clearer
  indicators; the rotor panel can associate one antenna and its heading offset
  with each band.
- CAT and CW: corrected CW macro delivery through Yaesu CAT and made radio
  command rejection reasons more visible.
- Advanced CAT: when declared by the selected Hamlib model, Settings exposes
  data bits, stop bits, parity, handshake, DTR/RTS states and the CI-V address.
  Values are preserved and passed to `rigctld`; unsupported controls are not
  shown.
- Updates: DecoDXLog checks for a signed, OS- and architecture-compatible
  package in `elisir80/DecodxLog` first, then falls back to
  `iu8lmc/DecoDXLog`. Windows selects setup `.exe` files only, macOS `.dmg`
  files only and Linux `.AppImage` files only; fork packages are verified with
  a dedicated signing key before installation.
- Packaging: workflows create independent signed manifests for Windows x64,
  macOS Apple Silicon/Intel and Linux x86_64/aarch64. The Windows setup is
  published only after real-world testing of the MinGW artefact.
