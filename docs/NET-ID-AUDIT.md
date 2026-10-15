# Identità UDP locale — audit DXL-002

`NetController` crea un `net/id` casuale quando manca e lo usa per filtrare i
propri datagrammi. `CloudSettings` classifica esplicitamente `net/id` come
impostazione Device: non viene raccolto nel documento sincronizzabile e viene
ignorato anche se arriva dal server.

Verifiche presenti e passate:

- `cloudsettings/networkIdentityStaysOnThisMachine`: due file di impostazioni
  conservano identità diverse durante collect/apply;
- `cloudsettings/whatBelongsToThisMachineIsNotAcceptedFromOutside`: una porta
  o identità di rete remota non sovrascrive la configurazione locale;
- `contestnet/receiveFromAnotherPc`: il trasporto UDP riceve un messaggio da un
  altro ID e scarta il proprio messaggio riflesso.

Il 2026-10-15 sono stati rieseguiti `cloudsettings` e `udppipeline` con
`ctest --test-dir build -R '^(cloudsettings|udppipeline)$'`: 2/2 passati.

Questo copre la separazione dell'identità e il trasporto locale senza alterare
gli UUID dei QSO, che sono memorizzati nel database e non derivano da `net/id`.

Limite: non è stato eseguito un test fisico su due host distinti; il test UDP
usa loopback e la sincronizzazione delle impostazioni usa file temporanei.
