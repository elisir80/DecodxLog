# DecoDXLog 1.17.04 / Release notes

## Italiano

Questa versione raccoglie gli aggiornamenti dalla 1.17.02 alla 1.17.04.

- **DX Cluster:** ogni fonte ha il pulsante **Check**. La diagnosi distingue
  risoluzione DNS, apertura TCP e saluto del nodo, indicando quando controllare
  firewall o antivirus invece di riportare soltanto “connection closed”.
- **QSL ed export ADIF:** il popup del periodo riceve il fuoco della tastiera;
  dal log e dal menu File si puo' esportare un intervallo ADIF inclusivo, con
  conteggio dei QSO e segnalazione esplicita se il file non si apre o non viene
  scritto interamente.
- **CW e Stop:** **Ferma** annulla il CW gia' in coda, rilascia la linea del
  keyer locale e invia PTT OFF d'emergenza alla stessa radio che ha ricevuto la
  macro. Risposte tardive di rigctld, TCI e flrig non possono piu' riavviare
  una trasmissione fermata.
- **Protezione Yaesu:** con CAT/Hamlib diretto, il CW e' disabilitato sui
  modelli Yaesu per cui Hamlib sovrascriverebbe la memoria 1 del keyer. CAT per
  frequenza e modo continua a funzionare; per il CW configurare un keyer
  seriale separato o WinKeyer.
- **Macro e compatibilita':** F1–F12 mantengono l'indicazione della macro
  effettivamente inviata; eliminati gli avvisi Qt 6.9 sulle conversioni UTC.

## English

This release collects the changes from 1.17.02 through 1.17.04.

- **DX Cluster:** every source has a **Check** button. Diagnostics distinguish
  DNS resolution, TCP connection and node greeting, and identify when a
  firewall or antivirus should be checked rather than only reporting
  “connection closed”.
- **QSL and ADIF export:** the date-period popup receives keyboard focus; the
  log and File menu can export an inclusive ADIF date range with its QSO count,
  and report a file-opening or incomplete-write error explicitly.
- **CW and Stop:** **Stop** cancels queued CW, releases a local keyer line and
  sends an emergency PTT OFF to the same radio which received the macro. Late
  rigctld, TCI or flrig responses can no longer restart a stopped transmission.
- **Yaesu protection:** with direct CAT/Hamlib, CW is disabled for Yaesu models
  where Hamlib would overwrite keyer memory 1. CAT frequency and mode control
  remain available; configure a separate serial keyer or WinKeyer for CW.
- **Macros and compatibility:** F1–F12 retain the indication of the macro
  actually sent, and Qt 6.9 UTC-conversion warnings have been removed.
