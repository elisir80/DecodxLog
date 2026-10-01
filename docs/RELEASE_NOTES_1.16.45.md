# DecoDXLog v1.16.45

## English

### UDP integration and reliable shutdown

- Manual QSOs can now be forwarded as N1MM XML to HamConnect/HamAward. Configure
  `127.0.0.1:12060` under **Settings → Decodium connection → Forward to other programs**.
  The broadcaster uses the active station profile callsign, sends the N1MM band-edge value
  expected by HamAward (for example `14` for 20 m), and maps SSB to USB.
- Closing the application now stops UDP listeners, timers, network services and queued
  background work before object teardown. The CW keyer shutdown no longer waits forever on
  a blocked serial-worker call.

### Since v1.16.43

- Added direct MicroHAM ARCO / Yaesu GS-232 rotor control over TCP, USB or RS-232: read
  azimuth, point the antenna and stop it without Hamlib or rotctld.
- Improved ARCO position parsing for fragmented serial replies and GS-232A reply format.
- Added flexible Logbook field filtering, confirmation-coloured rows, and cross-log search.
- Restored 60 m as enabled by default in DXCC, Challenge and WAS statistics; operators who
  need strict ARRL-only counting can disable it per award.

## Italiano

### Integrazione UDP e chiusura affidabile

- I QSO registrati a mano possono ora essere inoltrati a HamConnect/HamAward come XML
  N1MM. In **Impostazioni → Collegamento a Decodium → Inoltra ad altri programmi** inserire
  `127.0.0.1:12060`. L'invio usa il nominativo del profilo stazione attivo, il bordo basso
  della banda che HamAward si aspetta (per esempio `14` sui 20 m) e trasmette SSB come USB.
- In uscita DecoDXLog ferma socket UDP, timer, servizi di rete e lavori in coda prima della
  distruzione degli oggetti. Il keyer CW non puo' piu' trattenere indefinitamente la chiusura
  su una chiamata bloccata del driver seriale.

### Novita' dalla v1.16.43

- Controllo diretto del rotore MicroHAM ARCO / Yaesu GS-232 via TCP, USB o RS-232: lettura
  dell'azimut, puntamento e STOP senza Hamlib ne' rotctld.
- Lettura ARCO piu' robusta con risposte seriali frammentate e formato GS-232A.
- Filtri liberi del Log, righe colorate secondo le conferme e ricerca negli altri log.
- I 60 metri tornano abilitati di serie nei conti DXCC, Challenge e WAS; chi vuole il conto
  strettamente ARRL puo' disabilitarli diploma per diploma.
