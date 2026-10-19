# DecoDXLog 1.17.29

## Italiano

Aggiornamenti dalla `1.17.27` alla `1.17.29`.

- Integrati gli aggiornamenti upstream della `1.17.28`, comprese le nuove traduzioni, la sincronizzazione della versione e il collegamento corretto alla pagina Club Log per richiedere la chiave API.
- Rafforzato il percorso CAT condiviso: gestione più sicura delle connessioni, riutilizzo della CAT condivisa e diagnostica più chiara per radio raggiungibile, non raggiungibile o in riconnessione.
- Estesa la configurazione CW/CAT per evitare interferenze tra controllo frequenza/modo e keyer, con supporto più affidabile per le configurazioni seriali locali.
- Migliorata la gestione delle transizioni radio e dei comandi CAT nel controller, inclusi recupero della connessione e stato coerente durante le operazioni CW.
- Aggiornato il pannello CW per riflettere correttamente lo stato del controllo e delle periferiche configurate.
- Aggiunti e ampliati test automatici per CAT condivisa, RigControl, RigController e scenari di riconnessione.
- La release multipiattaforma usa i workflow verificati per DMG macOS arm64/Intel, installer Windows x64 e AppImage Linux x86_64/aarch64, con smoke test e manifest firmati per l’aggiornamento automatico.

## English

Updates from `1.17.27` to `1.17.29`.

- Integrated the upstream `1.17.28` updates, including new translations, version synchronization, and the corrected Club Log link for requesting an API key.
- Strengthened shared CAT handling with safer connection management, more reliable reuse of the shared CAT endpoint, and clearer diagnostics for reachable, unreachable, and reconnecting radios.
- Extended CW/CAT configuration to prevent interference between frequency/mode control and the keyer, with more reliable handling of local serial configurations.
- Improved radio transition and CAT command handling in the controller, including connection recovery and consistent state during CW operations.
- Updated the CW panel so it accurately reflects configured control and peripheral state.
- Added and expanded automated tests for shared CAT, RigControl, RigController, and reconnection scenarios.
- The cross-platform release uses the verified workflows for macOS arm64/Intel DMGs, the Windows x64 installer, and Linux x86_64/aarch64 AppImages, with smoke tests and signed update manifests.

All requested runners must complete successfully and every expected asset must be verified as non-empty before publication is considered complete.
