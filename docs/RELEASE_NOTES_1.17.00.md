# DecoDXLog 1.17.00 / Release notes

## Italiano

Questa versione raccoglie gli aggiornamenti dalla 1.16.54 alla 1.17.00.

- **Finestre e pannelli:** le finestre staccate e quelle interne usano una
  cornice condivisa piu' sobria, con angoli arrotondati e ombra dove supportato.
  Su macOS i controlli rosso, giallo e verde sono a sinistra; su Windows e Linux
  resta la stessa estetica senza dipendere dal compositor. Il contenuto resta
  correttamente ritagliato e la finestra massimizzata mantiene bordi normali.
- **Menu Pannelli:** il menu appare vicino al comando che lo apre, non piu'
  all'estrema destra; se non entra sotto, resta comunque dentro la finestra.
  Il **Keyer vocale** e' apribile dalla lista Pannelli anche senza Contest Mode.
- **CW / SO2R:** **Ferma** interrompe il percorso che sta effettivamente
  trasmettendo. In una configurazione SO2R cio' include la radio alternativa,
  non soltanto la radio principale.
- **Band map:** un intervallo di frequenze transitorio o non valido non puo'
  piu' creare un modello Qt negativo e il relativo warning nel Terminale.
- **QSL:** LoTW, eQSL e QRZ possono controllare e importare le conferme di un
  periodo definito. Il controllo locale conserva i limiti scelti e non modifica
  il cursore dello scarico incrementale ordinario.
- **Distribuzione e aggiornamenti:** rilasci firmati, separati per sistema e
  architettura, permettono all'autoaggiornamento di scegliere solo il pacchetto
  compatibile: Windows x64, macOS Apple Silicon/Intel o Linux x86_64/aarch64.

## English

This release collects the changes from 1.16.54 through 1.17.00.

- **Windows and panels:** detached and internal windows now share a quieter
  frame with rounded corners and a shadow where supported. macOS uses the red,
  yellow and green controls on the left; Windows and Linux retain the same
  appearance without relying on a compositor. Content remains clipped correctly
  and maximised windows retain normal rectangular edges.
- **Panels menu:** the menu opens beside the command that invoked it rather
  than at the far right. If there is insufficient room below, it remains inside
  the application window. The **Voice keyer** is also available from Panels
  outside Contest Mode.
- **CW / SO2R:** **Stop** now stops the actual transmitting path. In an SO2R
  configuration this includes the alternate radio, not just the primary radio.
- **Band map:** a transient or invalid frequency range can no longer create a
  negative Qt model and its corresponding Terminal warning.
- **QSL:** LoTW, eQSL and QRZ can check and import confirmations for a chosen
  date range. Local validation preserves the selected limits without moving the
  normal incremental-download cursor.
- **Distribution and updates:** signed packages are separated by operating
  system and architecture so the updater selects only the compatible package:
  Windows x64, macOS Apple Silicon/Intel, or Linux x86_64/aarch64.
