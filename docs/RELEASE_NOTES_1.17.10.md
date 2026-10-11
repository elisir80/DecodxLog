# DecoDXLog 1.17.10 / Release notes

## Italiano

Questa versione raccoglie gli aggiornamenti dalla 1.17.08 alla 1.17.10.

- **Rimando selettivo via UDP:** dal Log seleziona uno o piu' QSO, fai clic destro e scegli
  **Rimanda via UDP**. DecoDXLog invia ai destinatari configurati (HamAward, HamConnect o altri)
  lo stesso contatto XML N1MM inviato quando nasce un QSO; non salva duplicati, non cambia il log e
  non mette in coda invii QSL. Il registro attivita' mostra il risultato e la voce resta disabilitata
  se manca una destinazione UDP valida.
- **Ricezione CW piu' affidabile:** l'ingresso audio usa il formato nativo realmente negoziato,
  con conversione esplicita a mono signed 16-bit e downmix corretto per le sorgenti stereo. Tono e
  velocita' ricevuta possono restare automatici oppure essere fissati separatamente dal keyer.
- **Pannelli e macro stabili:** le viste QML non eseguono callback dopo la chiusura; su macOS le
  macro CW F1-F12 funzionano con fn/Globe+F1-F12 e anche con Ctrl+F1-F12 sulle tastiere prive di fn.
- **Cluster web:** il nuovo pulsante a globo nella barra inferiore apre rapidamente
  decowebcluster.ft2.it nel browser.

## English

This release collects the changes from 1.17.08 through 1.17.10.

- **Selective UDP resend:** select one or more QSOs in the Log, right-click a selected row and choose
  **Resend via UDP**. DecoDXLog sends configured recipients (HamAward, HamConnect or others) the
  same N1MM XML contact used for newly logged QSOs; it does not create duplicates, alter the log or
  queue QSL delivery. The activity log reports the result, and the action remains disabled until a
  valid UDP destination is configured.
- **More reliable CW reception:** audio uses the actually negotiated native format, with explicit
  signed 16-bit mono conversion and correct stereo downmixing. Receive tone and speed can remain
  automatic or be locked independently of the transmit keyer.
- **Stable panels and macros:** QML views no longer run callbacks after closing; on macOS CW macros
  F1-F12 work with fn/Globe+F1-F12 and with Ctrl+F1-F12 on keyboards without fn.
- **Web cluster:** the new globe button in the bottom bar opens decowebcluster.ft2.it in the browser.
