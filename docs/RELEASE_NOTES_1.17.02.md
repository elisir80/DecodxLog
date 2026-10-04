# DecoDXLog 1.17.02 / Release notes

## Italiano

Questa versione raccoglie gli aggiornamenti dalla 1.17.00 alla 1.17.02.

- **Rotore:** i comandi arrivati al gateway restano nell'ordine ricevuto anche
  mentre la linea seriale e' occupata. Una sequenza di puntamento e stop non
  puo' quindi essere riordinata e mandare il rotore verso una posizione
  inattesa.
- **CW e macro:** F1–F12 indicano la macro realmente inviata. Ogni pulsante
  manda il proprio testo, non quello di F1. **Ferma** pulisce il buffer del
  WinKeyer subito; per CAT/Hamlib invia `stop_morse` e un PTT OFF di sicurezza
  per le radio/backend che non interrompono il loro buffer CW al primo comando.
- **Finestre e pannelli:** i controlli circolari rosso, giallo e verde sono ora
  disponibili anche su Windows e Linux, a sinistra, con le stesse azioni di
  macOS. I pannelli usano gli stessi controlli per chiudere e staccare o
  riagganciare una vista.

## English

This release collects the changes from 1.17.00 through 1.17.02.

- **Rotor:** commands received by the gateway retain their original order even
  while the serial line is busy. A sequence of heading and stop commands can no
  longer be reordered and send the rotor to an unexpected position.
- **CW and macros:** F1–F12 now indicate the macro that was actually sent. Each
  button sends its own text rather than F1's text. **Stop** flushes the
  WinKeyer buffer immediately; for CAT/Hamlib it sends `stop_morse` followed by
  a safety PTT OFF for radios or backends that do not interrupt their CW buffer
  on the first command.
- **Windows and panels:** the red, yellow and green circular controls are now
  available on Windows and Linux as well, on the left, with the same actions as
  macOS. Panels use the same controls to close, detach or attach a view.
