# DecoDXLog 1.17.26

## Italiano

Aggiornamenti dalla `1.17.25` alla `1.17.26`.

- **Appearance funzionante:** il menu del marchio ora apre il selettore dei temi con un popup
  esplicito, posticipato dopo la chiusura del menu principale. Questo evita che macOS, Windows o
  Linux lo richiudano immediatamente come clic esterno.
- **Liquid Glass selezionabile:** aggiunto il tema `Liquid Glass`, con superfici arrotondate,
  traslucenza controllata e fallback opaco sui sistemi senza materiale nativo; i temi classici
  restano disponibili.
- **Bundle macOS piu' stabile:** il rebuild locale rimuove i residui di un precedente
  `macdeployqt`, evita di caricare due copie di Qt e rifirma ad-hoc l'app. Il workflow di release
  pulisce i plugin Qt non usati, incorpora OpenSSL e verifica `libcrypto` e `libssl` nel DMG.
- **Cloud e OpenSSL:** le build ufficiali richiedono la crittografia OpenSSL e includono le
  librerie runtime nel pacchetto Windows, macOS e Linux; senza OpenSSL una build locale puo'
  continuare a funzionare, oppure puo' essere resa obbligatoria con
  `DECODXLOG_REQUIRE_OPENSSL=ON`.
- **Aggiornamento automatico multipiattaforma:** il setup Windows viene pubblicato come
  `DecoDXLog-1.17.26-windows-x64-setup.exe`; vengono pubblicati anche i manifest firmati,
  `DecoDXLog-1.17.26-macos-arm64.dmg`, `DecoDXLog-1.17.26-macos-x86_64.dmg`,
  `DecoDXLog-1.17.26-linux-x86_64.AppImage` e
  `DecoDXLog-1.17.26-linux-aarch64.AppImage`.

## English

Updates from `1.17.25` to `1.17.26`.

- **Working Appearance menu:** the brand menu now opens the theme selector through an explicit
  popup scheduled after the parent menu closes. This prevents macOS, Windows, or Linux from
  immediately treating it as an outside-click dismissal.
- **Selectable Liquid Glass:** added the `Liquid Glass` theme with rounded surfaces, controlled
  translucency, and an opaque fallback on systems without a native material; all classic themes
  remain available.
- **More reliable macOS bundles:** local rebuilds remove stale `macdeployqt` leftovers, avoid
  loading two Qt copies, and ad-hoc sign the app. The release workflow removes unused Qt
  plugins, bundles OpenSSL, and verifies that `libcrypto` and `libssl` are inside the DMG.
- **Cloud and OpenSSL:** official builds require OpenSSL and include the runtime libraries in the
  Windows, macOS, and Linux packages; local builds may remain minimal, or enforce the dependency
  with `DECODXLOG_REQUIRE_OPENSSL=ON`.
- **Cross-platform automatic updates:** Windows publishes
  `DecoDXLog-1.17.26-windows-x64-setup.exe`; signed manifests are published together with
  `DecoDXLog-1.17.26-macos-arm64.dmg`, `DecoDXLog-1.17.26-macos-x86_64.dmg`,
  `DecoDXLog-1.17.26-linux-x86_64.AppImage`, and
  `DecoDXLog-1.17.26-linux-aarch64.AppImage`.
