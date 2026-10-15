# Contest rules audit

Contest scoring, exchanges, multipliers, operating-time limits and Cabrillo
metadata are driven by `ContestRules` sheets. Unknown contest IDs remain
invalid and score zero; the application does not invent a score for an
unsupported contest.

Edition overrides may narrow supported bands or adjust documented deadlines
and operating limits. Invalid overrides make the rules invalid instead of
silently producing a plausible score. Cabrillo export now refuses a contest
session whose contest ID has no verified rule sheet. Free/activation sessions
without a contest ruleset can still use the generic export path.

Verified locally:

- unknown contests return invalid rules and zero score;
- exchange validation, duplicate/multiplier keys and category-specific points
  are covered by `contestrules` tests;
- edition overrides reject unsupported bands;
- Cabrillo writing and validation are covered by the Cabrillo tests;
- the application build remains successful after the export guard.

The catalog is intentionally conservative: adding a contest requires a
reviewed rule sheet and corresponding sample tests rather than a guessed
fallback.

## Verifica ripetuta — 16 ottobre 2026

```text
cmake --build build --parallel 4 --target tst_contestrules tst_cabrillo tst_activation
ctest --test-dir build --output-on-failure -R '^(contestrules|cabrillo|activation)$'
3/3 test superati
```

La misura conferma i percorsi attuali per regole, categorie/scambi, punteggio,
edizioni e validazione Cabrillo. Non è stata aggiunta una nuova gara senza
regolamento verificato; il catalogo resta volutamente conservativo.
