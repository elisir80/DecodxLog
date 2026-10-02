# La firma degli aggiornamenti

Dalla 1.16.30 DecoDXLog installa da solo solo i pacchetti che chi pubblica ha firmato.
Una release senza firma si vede lo stesso nella finestra dell'aggiornamento, ma
«Aggiorna ora» non compare: la si scarica a mano dalla pagina.

## Come funziona

Accanto a ciascun pacchetto, ogni release ha due file specifici per piattaforma e
architettura, per esempio per Windows x64:

- `decodxlog-release-windows-x86_64.json` — repository, versione, e per ogni file nome, dimensione e
  SHA-256;
- `decodxlog-release-windows-x86_64.json.sig` — la firma Ed25519 (RFC 8032) di quel file, con l'id
  della chiave.

Gli altri nomi sono `...-macos-arm64`, `...-macos-x86_64`,
`...-linux-x86_64` e `...-linux-aarch64`. Il vecchio nome generico resta
letto per compatibilita' con le release precedenti.

Il programma conosce le chiavi pubbliche di chi pubblica, una lista per repository
(`src/core/ReleaseSignature.cpp`, `trustedKeys()`). Prima di proporre «Aggiorna ora»
scarica i due file, controlla la firma con la chiave di quel repository, controlla che
repository e versione siano proprio quelli della release, e prende SHA-256 e dimensione
del pacchetto. Poi scarica il pacchetto e lo tiene solo se lo SHA-256 torna: su Linux
l'AppImage che gira non viene toccata prima del controllo.

Prima si prova `elisir80/DecodxLog`: deve offrire un pacchetto del tipo e
dell'architettura del computer (setup `.exe` Windows, `.dmg` macOS,
`.AppImage` Linux) e la relativa firma valida. Solo se manca una release cosi'
utilizzabile si passa a `iu8lmc/DecoDXLog`. Se nessuna firma e' valida, il
programma puo' solo proporre il download manuale.

## Le chiavi di iu8lmc/DecoDXLog

| id | cos'e' | dove sta |
|---|---|---|
| `ad518c5309aee583` | quella di tutti i giorni | Gestione credenziali di Windows di chi pubblica, servizio «DecoDXLog release signing», nome `main` |
| `411be55f604c83ef` | quella di scorta | un file, da tenere fuori dal computer (chiavetta, cassaforte) |

La chiave segreta non sta mai nel repository, non si stampa e non si manda a nessuno.
Se quella di tutti i giorni si perde, si firma con quella di scorta
(`KEY_FILE=... scripts/sign-release.sh`) e si pubblica una versione con una chiave nuova
in `trustedKeys()`. Se si perdono tutte e due, i programmi gia' installati non
accettano piu' aggiornamenti automatici: si reinstalla a mano una volta.

## Pubblicare

Dopo `scripts/deploy.sh` e `scripts/installer.sh`:

    BUILD=/c/decolog/build-rel scripts/sign-release.sh

scrive `decodxlog-release.json` e `.sig` nella cartella del progetto e li controlla
subito con le chiavi scritte nel programma. Si caricano sulla release insieme al setup
e allo zip. Ogni file aggiunto dopo (i pacchetti macOS e Linux) va firmato di nuovo,
tutti insieme, perche' l'elenco e' uno solo.

Lo strumento e' `decodxlog_sign` (`tools/releasesign.cpp`): `keygen`, `export`,
`import`, `public`, `sign`, `verify`. `decodxlog_sign --help` li elenca.

## Un fork che pubblica i suoi pacchetti

La firma di un repository non vale per le release di un altro. Chi pubblica da un fork:

1. crea la sua chiave: `decodxlog_sign keygen --name main` (e una di scorta con
   `--name recovery --export FILE`);
2. aggiunge le chiavi pubbliche in `trustedKeys()` con il nome del suo repository;
3. firma le sue release con `decodxlog_sign sign --repository owner/progetto ...`.

Senza questi passi le release del fork si vedono, ma si installano a mano.

## Il fork elisir80/DecodxLog

Il fork usa la chiave pubblica `9194bfce15e1a728`, gia' inserita nel
programma. La chiave privata e' nel Secret GitHub
`DECODXLOG_ELISIR80_RELEASE_SIGNING_KEY`, non nel repository. I workflow di
macOS e Linux firmano il rispettivo asset; quello Windows costruisce con MinGW
e pubblica `DecoDXLog-<version>-windows-x64-setup.exe` soltanto con
`publish=true`, dopo la prova reale dell'artefatto scaricato.

## Controllare a mano

    decodxlog_sign verify --repository iu8lmc/DecoDXLog --version 1.16.30 \
        DecoDXLog-1.16.30-setup.exe

nella cartella dove stanno i pacchetti e i due file della firma.
