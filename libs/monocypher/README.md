# Monocypher

La libreria crittografica di Loup Vaillant, presa da
<https://github.com/LoupVaillant/Monocypher> e tenuta qui dentro invece che come
dipendenza di sistema: sono quattro file C, nessuna dipendenza, e compilano uguali
su Windows, macOS e Linux. A DecoDXLog serve una cosa sola: la firma Ed25519
(RFC 8032, con SHA-512) che dice se un aggiornamento viene davvero da chi pubblica
DecoDXLog.

- versione: 4.0.2 (tag `4.0.2`, archivio del tag su GitHub)
- file: `src/monocypher.c`, `src/monocypher.h`, `src/optional/monocypher-ed25519.c`,
  `src/optional/monocypher-ed25519.h`, piu' `LICENCE.md`
- licenza: BSD a due clausole o CC-0, a scelta — in `LICENCE.md`

SHA-256 dei file come sono arrivati:

    02174117935699d418443c75a558a287deb06ef8cf7c1adced61d9047d2f323d  monocypher.c
    fcaf6ed771358bb4f40fba016f6518ae86ec02b1b877d2cc35ad92d3a26fd7b3  monocypher.h
    97d581639dfa72be08a6d57deb7d79b736be001cb416819cab196d22559d242b  monocypher-ed25519.c
    3a3035181f991a158d0e1c7567258f0bae8ba0f1f23c5512b4a1db1b3c9730ce  monocypher-ed25519.h

Non e' stato cambiato niente. I vettori ufficiali dell'RFC 8032 stanno in
`tests/tst_updates.cpp`: se una versione nuova li sbaglia, il test lo dice.
