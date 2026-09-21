# ggmorse

Il decodificatore Morse di Georgi Gerganov, preso da
<https://github.com/ggerganov/ggmorse> e tenuto qui dentro invece che come
sottomodulo: sono otto file e nessuna dipendenza, e cosi' DecoDXLog si compila
anche senza rete.

- versione: commit `7b4822a8cfdbb1addfe497f3ae8186f142a4ee79` (24 agosto 2026)
- licenza: MIT, in `LICENSE` — la stessa che si trova nel repository originale

## Cosa e' stato cambiato

Una cosa sola, in `src/ggmorse.cpp`: la libreria stampava su `stdout` ogni
carattere mentre lo decodificava. Un programma con la finestra non ha un
terminale dove stamparlo, e il testo ce lo prendiamo da `takeRxData()`. Le due
`printf` adesso passano da `GGMORSE_PRINT`, che con `GGMORSE_QUIET` definito —
come lo definisce il nostro CMake — non fa niente.

Tutto il resto e' come nell'originale: se esce una versione nuova, si ricopiano
i file e si rifa' quella sola modifica.

## Come lo usa DecoDXLog

`src/core/CwDecoder.cpp`. Il decodificatore riceve l'audio della radio a
pezzetti, lo gira a ggmorse e ne tira fuori le lettere, il tono trovato e la
velocita'.
