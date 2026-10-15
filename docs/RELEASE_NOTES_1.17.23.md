# DecoDXLog 1.17.23

## Italiano

Aggiornamenti dalla `1.17.22` alla `1.17.23`.

- Corretto il controllo di flusso CAT quando è impostato su RTS: per le connessioni seriali interessate viene usato `None`, evitando che la radio si scolleghi continuamente.
- Migliorata la gestione delle porte seriali su Linux: i nomi dei device mantengono la distinzione tra maiuscole e minuscole.
- Estesi i test multipiattaforma per le porte riservate, i nomi seriali e la connessione radio.
- Mantenuti i miglioramenti della 1.17.22 per porte riservate, CAT/PTT, WinKeyer, DecoPort e riavvio controllato di `rigctld`.

## English

Updates from `1.17.22` to `1.17.23`.

- Fixed CAT flow control when RTS is selected: affected serial connections now use `None`, preventing repeated radio disconnects.
- Improved Linux serial-port handling: device names preserve case sensitivity.
- Extended cross-platform tests for reserved ports, serial names and radio connection behavior.
- Preserved the 1.17.22 improvements for reserved ports, CAT/PTT, WinKeyer, DecoPort and controlled `rigctld` restarts.
