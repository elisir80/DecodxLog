# DecoDXLog 1.16.54 / Release notes

## Italiano

Questa versione raccoglie gli aggiornamenti dalla 1.16.48 alla 1.16.54.

- **Chiusura su macOS:** chiudere la finestra principale arresta ora in modo
  esplicito audio, CAT, CAT condivisa, scansioni radio e i processi `rigctld`
  avviati da DecoDXLog. Un processo CAT bloccato non deve piu' lasciare
  l'applicazione attiva nel Terminale.
- **Monitor Decodium:** nuovo monitor del traffico per UDP, DecoLink e
  DecoPort, nei due versi, con una vista immediata dei pacchetti e dei client
  collegati.
- **Lavagna Decodium:** il pannello Decodium include ricevitori compatti Full
  Spectrum e Signal RX, finestre native specchiabili e storico delle decodifiche.
- **Audio CW e DVK:** la scheda audio selezionata viene rispettata con
  precisione; non viene piu' usato silenziosamente un ingresso alternativo.
- **QSL e conferme:** il riepilogo degli scarichi eQSL e LoTW resta disponibile
  anche in caso di errore e mostra in modo piu' chiaro le cartoline e le
  conferme ricevute.
- **Form QSO:** il rapporto proposto segue il modo operativo e le nuove
  finestre di importazione e riepilogo semplificano la gestione del registro.
- **Rotore:** il gateway integrato supporta anche Yaesu G-450 tramite protocollo
  GS-232, oltre ai dispositivi gia' supportati.
- **Distribuzione:** restano disponibili pacchetti firmati e specifici per
  Windows x64, macOS Apple Silicon/Intel e Linux x86_64/aarch64, selezionati
  dall'autoaggiornamento in base al sistema e all'architettura.

## English

This release collects the changes from 1.16.48 through 1.16.54.

- **macOS shutdown:** closing the main window now explicitly stops audio, CAT,
  shared CAT, radio probes and any `rigctld` process started by DecoDXLog. A
  stalled CAT process should no longer leave the application alive in Terminal.
- **Decodium monitor:** a new bidirectional traffic monitor covers UDP,
  DecoLink and DecoPort, with immediate packet and connected-client visibility.
- **Decodium workspace:** the Decodium panel adds compact Full Spectrum and
  Signal RX receivers, mirrorable native windows and decode history.
- **CW and DVK audio:** the chosen audio device is honoured precisely; the
  application no longer silently falls back to a different input.
- **QSL and confirmations:** the eQSL and LoTW download summary remains
  available after an error and presents cards and received confirmations more
  clearly.
- **QSO entry:** suggested reports now follow the operating mode, while new
  import and summary windows simplify log management.
- **Rotor:** the integrated gateway now supports Yaesu G-450 controllers using
  the GS-232 protocol, alongside the existing supported devices.
- **Distribution:** signed platform-specific packages remain available for
  Windows x64, macOS Apple Silicon/Intel and Linux x86_64/aarch64. The updater
  selects the package that matches the current operating system and architecture.
