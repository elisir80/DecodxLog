# DecoDXLog 1.17.15 / Release notes

## Italiano

Questa versione raccoglie gli aggiornamenti dalla 1.17.10 alla 1.17.15.

- **CW piu' leggibile e robusto:** oltre al decoder classico, la build puo' usare opzionalmente il
  modello neurale CTC di Morsecodium; il pannello mostra la fiducia della lettura, corregge con
  prudenza le parole del lessico CW e visualizza punti, linee e rapporto del pugno misurati.
- **Piu' stazioni CW insieme:** il pannello trova fino a cinque segnali fra 300 e 1450 Hz, ciascuno
  con tono e decoder propri. Un clic ascolta una stazione, doppio clic porta il nominativo nel QSO e
  il tasto destro prepara lo spot. Con un tono fissato, un passa-banda stretto aiuta contro vicini forti.
- **Log e QSO piu' rapidi:** le righe del Log si selezionano trascinando, con Ctrl per aggiungere e
  Ctrl+A per selezionare quanto mostrato. I nominativi nel testo CW sono cliccabili e il testo
  decodificato puo' essere selezionato e copiato senza perdere la selezione quando arrivano caratteri.
- **Radio senza blocchi:** ricerca radio, elenco dei modelli Hamlib, opzioni seriali e avvio di
  rigctld lavorano fuori dal filo della finestra; Impostazioni resta reattiva anche con radio spenta,
  porta occupata o Hamlib lento.
- **Continuita' 1.17.10:** resta disponibile il reinvio selettivo via UDP dei QSO gia' nel Log verso
  HamAward, HamConnect e altri destinatari configurati, senza creare duplicati locali o accodare QSL.

## English

This release collects the changes from 1.17.10 through 1.17.15.

- **More readable and robust CW:** alongside the classic decoder, builds can optionally use
  Morsecodium's CTC neural model. The panel shows decoding confidence, carefully corrects CW lexicon
  words, and displays measured dots, dashes and fist ratio.
- **Multiple CW stations at once:** the panel detects up to five signals between 300 and 1450 Hz,
  each with its own tone and decoder. Click to listen, double-click to put the callsign into the QSO,
  or right-click to prepare a spot. A narrow band-pass filter helps against strong adjacent signals
  when a tone is locked.
- **Faster log and QSO work:** Log rows can be selected by dragging, with Ctrl to add and Ctrl+A to
  select all visible rows. Callsigns in CW text are clickable, and decoded text can be selected and
  copied without losing the selection as new characters arrive.
- **Non-blocking radio setup:** radio discovery, Hamlib model and serial-option lists, and rigctld
  startup run outside the UI thread, keeping Setup responsive when a radio is off, a port is busy or
  Hamlib is slow.
- **1.17.10 continuity:** selected QSOs can still be resent via UDP to HamAward, HamConnect and other
  configured recipients without creating local duplicates or queueing QSL delivery.
