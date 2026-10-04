# DecoDXLog 1.17.03 / Release notes

## Italiano

Questa versione raccoglie gli aggiornamenti dalla 1.17.02 alla 1.17.03.

- **Diagnosi DX Cluster:** ogni fonte ha il pulsante **Check**, che distingue
  risoluzione DNS, apertura della porta e saluto del nodo. I messaggi indicano
  quando verificare firewall o antivirus, anziche' limitarsi a “connection
  closed”.
- **QSL ed export ADIF:** il popup del periodo riceve correttamente il fuoco
  della tastiera. Dal log e dal menu File e' possibile esportare un intervallo
  di date ADIF, con estremi inclusi e conteggio dei QSO. Un errore di apertura
  o scrittura del file viene ora segnalato esplicitamente.
- **CW sicuro e arresto piu' affidabile:** **Ferma** annulla il CW gia' in
  coda, rilascia la linea del keyer locale e invia un PTT OFF d'emergenza alla
  stessa radio che aveva ricevuto la macro. Questo protegge anche da risposte
  tardive di rigctld, TCI e flrig.
- **Yaesu con CAT/Hamlib diretto:** il CW viene disabilitato esplicitamente
  per i modelli Yaesu per cui Hamlib sovrascriverebbe la memoria 1 del keyer.
  CAT per frequenza e modo continua a funzionare; per trasmettere CW si usa un
  keyer seriale separato oppure WinKeyer.
- **Compatibilita':** eliminati gli avvisi Qt 6.9 sulle conversioni UTC e
  aggiornati i valori predefiniti dei workflow manuali a `v1.17.03`.

## English

This release collects the changes from 1.17.02 through 1.17.03.

- **DX Cluster diagnostics:** every source has a **Check** button which
  distinguishes DNS resolution, TCP connection and the node greeting. Messages
  now identify when a firewall or antivirus should be checked rather than only
  reporting “connection closed”.
- **QSL and ADIF export:** the date-period popup correctly receives keyboard
  focus. The log and File menu can export an inclusive ADIF date range and
  display its QSO count. File opening or writing errors are now reported
  explicitly.
- **Safer CW and more reliable stop:** **Stop** cancels queued CW, releases a
  local keyer line and sends an emergency PTT OFF to the same radio which
  received the macro. It also prevents late rigctld, TCI and flrig replies from
  restarting a transmission.
- **Direct Yaesu CAT/Hamlib:** CW is explicitly disabled for Yaesu models where
  Hamlib would overwrite keyer memory 1. CAT frequency and mode control remain
  available; use a separate serial keyer or WinKeyer for CW transmission.
- **Compatibility:** Qt 6.9 UTC-conversion warnings have been removed and the
  manual-release workflows now default to `v1.17.03`.
