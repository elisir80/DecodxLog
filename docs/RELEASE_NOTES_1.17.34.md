# DecoDXLog 1.17.34

## Italiano

Aggiornamenti dalla `1.17.31` alla `1.17.34`.

### Dalla 1.17.31 alla 1.17.33

- **1.17.31:** CAT condivisa con inoltro del livello ALC, polling e capability Hamlib; aggiunti
  anche SWR, potenza RF, stato alimentazione e ATU con controlli protetti.
- **1.17.32:** rafforzata la CAT condivisa, con diagnostica piu' chiara per radio raggiungibile,
  non raggiungibile o in riconnessione e minori interferenze fra controllo radio e keyer.
- **1.17.33:** aggiunta la gestione di log dedicati a manifestazioni, contest e diplomi, con
  cancellazione recuperabile nel Cestino e traduzioni aggiornate.

- **Nuovo QSO configurabile:** il pannello e la finestra Nuovo QSO permettono di scegliere i
  campi opzionali da visualizzare, inclusi stato/provincia e potenza TX.
- **Campi ADIF personalizzati:** aggiunto un editor per campi ADIF liberi. I nomi vengono
  normalizzati e verificati; i campi fondamentali o gestiti da DecoDXLog non possono essere
  sovrascritti. I valori vengono conservati in `adif_extra` e tornano nell'esportazione ADIF.
- **Inserimento piu' rapido:** la potenza TX predefinita viene dal profilo stazione, il rapporto
  inviato viene inizializzato automaticamente e `Alt+W` pulisce il nuovo QSO.
- **Cluster e portachiavi:** una password non ancora salvata non provoca piu' una richiesta
  inutile al portachiavi. L'interfaccia distingue password mancante, errore di lettura e
  salvataggio in corso; dopo un salvataggio riuscito la connessione viene riavviata.
- **Diagnostica connessioni:** il test del nodo spiega che il prompt `login:` viene controllato
  senza autenticare l'utente.
- **Test multipiattaforma:** aggiunti test core, cluster, diagnostica e QML per le nuove funzioni.

## English

Updates from `1.17.31` to `1.17.34`.

### From 1.17.31 to 1.17.33

- **1.17.31:** shared CAT forwarding for the ALC level, Hamlib polling and capability reporting;
  SWR, RF power, power state, and ATU controls were also exposed with protected controls.
- **1.17.32:** strengthened shared CAT handling, with clearer diagnostics for reachable,
  unreachable, and reconnecting radios and fewer conflicts between radio control and the keyer.
- **1.17.33:** added dedicated logs for events, contests, and awards, with recoverable Trash
  deletion and updated translations.

- **Configurable New QSO:** the compact panel and the New QSO window can choose which optional
  fields to display, including state/province and TX power.
- **Custom ADIF fields:** added an editor for arbitrary ADIF fields. Names are normalized and
  validated; core or DecoDXLog-managed fields cannot be overwritten. Values are kept in
  `adif_extra` and preserved in ADIF export.
- **Faster entry:** the default TX power comes from the active station profile, the sent report
  is initialized automatically, and `Alt+W` clears the New QSO form.
- **Cluster credentials:** an unset password no longer triggers an unnecessary keychain prompt.
  The UI distinguishes a missing password, a read error, and an in-progress save; a successful
  save restarts the connection automatically.
- **Connection diagnostics:** node checks explain that the `login:` prompt is intentionally
  checked without authenticating the user.
- **Cross-platform tests:** added core, cluster, diagnostics, and QML tests for the new behavior.
