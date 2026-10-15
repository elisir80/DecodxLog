# DecoDXLog 1.17.20

## English

This release carries the work accumulated after 1.17.15 through the 1.17.20 release line.

### Highlights

- Expanded award and contest rule coverage, with shared fixtures and regression tests for award scopes, DXCC/WAC handling, contest rules, and country normalization.
- Improved QSO, backup, restore, QSL, cloud, and offline-data handling, including reconciliation and migration checks.
- Added diagnostics and release-gate checks used before packaging, plus architecture, API, accessibility, integration, and performance audit coverage.
- Improved activation-reference validation and network/cloud error reporting while preserving existing sessions on rejected input.
- Refined QML dialogs, first-run guidance, bottom tabs, QSO details, and award workflows.
- Kept the cross-platform packaging workflows for macOS arm64/x86_64, Windows x64, and Linux x86_64/aarch64, including signed update manifests where the repository secret is configured.
- Fixed the non-ONNX CTC build so ONNX-only constants and beam-search code are not compiled when the feature is disabled; the focused CTC test passes.

### Verification

- Core build completed on macOS with Qt.
- The `ctcdecoder` test passed.
- `git diff --check` passed.
- Release workflows remain manual and must be checked by their workflow conclusion and non-empty release assets.

## Italiano

Questa versione raccoglie il lavoro maturato dopo la 1.17.15 nella linea di rilascio 1.17.20.

### Aggiornamenti principali

- Estesa la copertura di diplomi e regole contest, con fixture condivise e test di regressione per ambiti award, DXCC/WAC, contest e normalizzazione dei paesi.
- Migliorata la gestione di QSO, backup, ripristino, QSL, cloud e dati offline, comprese verifiche di riconciliazione e migrazione.
- Aggiunti diagnostica e controlli di release gate per il packaging, insieme a copertura di audit architetturale, API, accessibilità, integrazioni e prestazioni.
- Migliorate la validazione dei riferimenti di attivazione e la diagnostica degli errori di rete/cloud, preservando la sessione precedente quando un input viene rifiutato.
- Rifiniti i dialog QML, la guida iniziale, le schede inferiori, i dettagli QSO e i flussi degli award.
- Mantenuti i workflow multipiattaforma per macOS arm64/x86_64, Windows x64 e Linux x86_64/aarch64, inclusi i manifest firmati per l’aggiornamento quando la chiave del repository è configurata.
- Corretto il build CTC senza ONNX: costanti e beam search riservati a ONNX non vengono più compilati quando la funzione è disabilitata; il test CTC dedicato passa.

### Verifica

- Build del core completata su macOS con Qt.
- Test `ctcdecoder` superato.
- `git diff --check` superato.
- I workflow di release restano manuali e vanno verificati tramite conclusione del workflow e asset non vuoti effettivamente presenti nella release.
