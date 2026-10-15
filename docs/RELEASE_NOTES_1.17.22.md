# DecoDXLog 1.17.22

## Italiano

Aggiornamenti dalla `1.17.20` alla `1.17.22`.

- Corretta l’importazione ADIF, con anteprima funzionante e pulsante «Importa» operativo.
- Ripristinata la scheda inferiore con Diplomi, Statistiche, QSL, Attività, Cluster e Propagazione.
- Corrette anteprima importazione e diagnostica integrazioni con il tema Ocean Blue.
- Aggiornate le traduzioni e aggiunti test QML per importazione, FileDialog e finestre tematizzate.
- Aggiunta la gestione delle porte seriali riservate: le porte di Decodium, CAT, PTT, amplificatore, CAT nativo e WinKeyer non vengono più assegnate impropriamente ad altri componenti.
- Migliorata la ricerca delle radio seriali, evitando estremi virtuali nascosti e gestendo la porta virtuale DecoPort.
- Cambiare modello, porta o velocità seriale riavvia correttamente `rigctld`; i comandi «Collega» e «Trova la radio» funzionano anche nella modalità seriale.
- Aggiunti test dedicati per le porte seriali e il controller radio.

## English

Updates from `1.17.20` through `1.17.22`.

- Fixed ADIF import with a working preview and operational “Import” button.
- Restored the bottom tab containing Awards, Statistics, QSL, Activity, Cluster and Propagation.
- Fixed the import preview and integration diagnostics under the Ocean Blue theme.
- Updated translations and added QML tests for import actions, FileDialog usage and themed windows.
- Added reserved serial-port handling: Decodium, CAT, PTT, amplifier, native CAT and WinKeyer ports are no longer assigned incorrectly to other components.
- Improved serial-radio discovery by avoiding hidden virtual endpoints and handling the DecoPort virtual port.
- Changing the model, port or serial speed now correctly restarts `rigctld`; “Connect” and “Find radio” also work in serial mode.
- Added focused tests for serial ports and the radio controller.
