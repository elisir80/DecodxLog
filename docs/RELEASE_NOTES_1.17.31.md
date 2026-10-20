# DecoDXLog 1.17.31 — Release notes / Note di rilascio

## English

### Changes since 1.17.29

#### 1.17.30

- Cloud synchronization now shows progress while the log is uploaded.
- The progress bar and percentage remain consistent when new QSOs are added during a sync.
- Cloud UI and diagnostics now report the upload state more clearly.

#### 1.17.31

- Added Hamlib CAT polling and forwarding for `ALC`.
- Added the extended CAT response format expected by Decodium: `get_level: ALC`, `Level Value`,
  and `RPRT 0`.
- Declared ALC, SWR, and forward-power meter capabilities in the shared CAT state.
- Shared CAT now exposes SWR and RF power in watts when the selected radio/driver provides them.
- Added CAT power-state and tuner controls: power on/off and ATU on/off.
- Control permissions are enforced: power/tuner changes require CAT control; enabling the tuner
  also requires transmit permission because it may key the radio.
- Unsupported capabilities return standard Hamlib `RPRT -11` instead of fabricated values.
- Added focused CAT/Rig tests for ALC, meter forwarding, power state, and tuner commands.

ALC remains radio- and driver-dependent and is normally meaningful only while transmitting.

## Italiano

### Modifiche dalla 1.17.29

#### 1.17.30

- La sincronizzazione Cloud mostra ora l’avanzamento mentre il log viene caricato.
- Barra e percentuale restano coerenti se durante la sincronizzazione vengono aggiunti nuovi QSO.
- Interfaccia e diagnostica Cloud indicano più chiaramente lo stato del caricamento.

#### 1.17.31

- Aggiunto il polling CAT Hamlib del livello `ALC`.
- Aggiunto il formato CAT esteso atteso da Decodium: `get_level: ALC`, `Level Value` e `RPRT 0`.
- Le capability CAT condivise dichiarano ora ALC, SWR e misuratore di potenza RF.
- La CAT condivisa espone SWR e potenza RF in watt quando radio e driver lo supportano.
- Aggiunti i comandi CAT per accendere/spegnere la radio e abilitare/disabilitare l’ATU.
- I permessi sono rispettati: i cambiamenti power/tuner richiedono il controllo CAT; l’attivazione
  dell’ATU richiede anche il permesso di trasmissione perché può mettere la radio in TX.
- Le capability non supportate rispondono con il normale `RPRT -11` Hamlib, senza inventare valori.
- Aggiunti test mirati per ALC, meter, stato power e comandi ATU.

ALC resta dipendente dalla radio e dal driver Hamlib ed è normalmente significativo solo durante la TX.
