# Fixture comune desktop/Cloud — audit DXL-028

## Verifica

`tests/fixtures/awards_shared.json` è il corpus comune letto dal test desktop
`tests/tst_awards.cpp` e dal test Cloud `server/tests/test_analytics.py`.
Contiene gli stessi due QSO, filtri di banda e modo, conteggi DXCC/WAC/WAZ,
statistiche e stato LoTW. Entrambi i lati leggono gli attesi dalla fixture;
non esistono valori duplicati nei due test.

Comandi eseguiti sul ramo corrente:

```text
ctest --test-dir build -R '^awards$' --output-on-failure
1/1 passed

pytest -q server/tests/test_analytics.py
21 passed
```

## Esito e limite

La fixture dimostra il contratto condiviso per il corpus coperto e rende
visibile una divergenza come fallimento del test. Non dimostra ancora la
parità completa di tutte le regole, diplomi e filtri: prima di considerare
DXL-028 completamente chiuso va esteso il corpus alle correzioni successive
e mantenuto in CI su entrambe le implementazioni.

## Verifica ripetuta — 16 ottobre 2026

Sul checkout corrente sono stati rieseguiti entrambi i lati del contratto:

```text
ctest --test-dir build --output-on-failure -R '^awards$'
1/1 passed

pytest -q server/tests/test_analytics.py
21 passed
```

La voce resta parzialmente verificata: non è stata dichiarata parità globale
finché il corpus condiviso non coprirà tutte le regole e i filtri supportati.
