# DecoLog — Fase 0: specifica di partenza

Stato: bozza 0.1 · IU8LMC · settembre 2026

---

## 1. Decisioni di base

**DecoLog si scrive da zero.** QLog (OK1MLG) e Wavelog servono solo come riferimento funzionale: si guarda *cosa* fanno, non *come*. Nessun file, classe o schema viene copiato. Questo tiene DecoLog libero da vincoli di fork e coerente al 100% con l'architettura Decodium.

- **Licenza:** GPL-3.0, come Decodium 4.0 Core Shannon.
- **Due prodotti:**
  - **DecoLog Desktop**: il log principale, offline-first, sul PC della stazione.
  - **DecoLog Cloud**: sync tra dispositivi, backup, accesso web, punto d'arrivo per Decodium Mobile.
- **Famiglia Decodium:** stesso tema, stessi componenti, stessa sensazione d'uso. Chi apre DecoLog accanto a Decodium deve vedere *un solo strumento*, come già succede con DecoRTTY.
- **FT2 è un modo di prima classe**, non un'aggiunta: `MODE=MFSK`, `SUBMODE=FT2`, come già scritto in `logbook/logbook.cpp` di Decodium (ratificato in ADIF 3.1.7).

> ⚠️ Correzione rispetto ai documenti inviati a Paul: lì la codifica FT2 in ADIF risultava "da definire". È già ufficiale. Il punto va tolto da Fase 0 e dal planning.

---

## 2. Cosa prendere come spunto da QLog

Catalogo delle funzioni viste in QLog, filtrate per DecoLog. **MVP** = entro la 1.0.

| Area | Funzione | Priorità | Note per DecoLog |
|---|---|---|---|
| Log | Tabella QSO con colonne configurabili, filtri salvati | MVP | Stile righe e densità di Decodium |
| Log | Scheda nuovo QSO manuale | MVP | Per SSB/CW, i digitali arrivano da Decodium |
| Log | Dettaglio / modifica QSO | MVP | |
| Log | Profili stazione (call, locatore, potenza, antenna) | MVP | Indispensabile per portatile ed eventi speciali |
| Integrazione | Ricezione QSO da WSJT-X/Decodium via UDP | MVP | Vedi §3 |
| Import/Export | ADIF import/export, recupero ADIF | MVP | Lossless, vedi §6 |
| Sicurezza | Credenziali in keystore di sistema | MVP | qtkeychain |
| Lookup | Callbook QRZ.com / HamQTH | 1.x | |
| QSL | LoTW (TQSL locale), QRZ Logbook, Club Log, eQSL | 1.x | Upload + download conferme |
| Award | DXCC, WAS, WAZ, WPX, Grid, IOTA, POTA/SOTA/WWFF | 1.x | **FT2 Award** integrato fin da subito |
| Statistiche | Per banda/modo/continente, mappa QSO | 1.x | Riusare lo stile di MapStatisticsPanel |
| DX | DX cluster, bandmap | 2.x | Decodium ha già molto lato DX |
| Rig | CAT diretto (Hamlib) | 2.x | Nel MVP il CAT resta a Decodium |
| Rotore | Controllo rotore | 2.x | Delegare a DecoRotor |
| CW | Keyer, console CW | Fuori scope | |
| Contest | Cabrillo, dupe check | 2.x | |
| QSL cartacee | Etichette, galleria QSL | 2.x | |

Principio generale: **DecoLog non duplica ciò che Decodium o gli altri Deco* già fanno** (CAT, rotore, decodifica). Si parla con loro.

---

## 3. Integrazione con Decodium

### 3.1 Canale MVP: protocollo UDP esistente

Decodium eredita il protocollo UDP di WSJT-X (`Network/NetworkMessage.hpp`):

- magic `0xadbccbda`, schema 3, serializzazione `QDataStream`;
- messaggi utili: **`QSOLogged`** (campi strutturati) e **`LoggedADIF`** (record ADIF completo);
- `Status` per frequenza, modo e call DX correnti, da mostrare in DecoLog senza toccare il CAT.

DecoLog ascolta su una porta configurabile (default 2237) e usa **`LoggedADIF` come fonte primaria**, perché è lossless. Vantaggio: DecoLog funziona subito anche con WSJT-X e JTDX, il che ne allarga la base utenti.

### 3.2 Canale futuro: DecoLink locale

Un canale bidirezionale dedicato Decodium ⇄ DecoLog (TCP locale o WebSocket) per:
- worked-before e stato conferme mostrati *dentro* Decodium mentre si decodifica;
- stato award FT2 in tempo reale;
- conferma di scrittura (ACK), così Decodium sa che il QSO è salvato.

Da progettare in Fase 1, non blocca l'MVP.

### 3.3 Da verificare nel codice Decodium

- `Network/DecodiumCloudlogLite.*` e `DecodiumQrzLogbookLite.*`: Decodium sa già caricare su Cloudlog e QRZ. Un `DecodiumDecoLogLite` sul modello di questi è la via più breve per l'upload diretto da Decodium Mobile verso il Cloud.
- `logbook/WorkedBefore.*`: capire se DecoLog può diventare la fonte del worked-before.

---

## 4. Tema e layout

### 4.1 Fonte di verità: `DecodiumThemeManager`

DecoLog **non definisce colori propri**. Usa gli stessi token di `src/ui/DecodiumThemeManager.h`.

| Token | Ocean Blue (default) | Stellar Light | Darkcodium |
|---|---|---|---|
| bgDeep | `#0A0F1A` | `#EDF2F7` | `#050706` |
| bgMedium | `#111827` | `#E1E9F1` | `#0d1310` |
| bgLight | `#1E2D42` | `#FFFFFF` | `#182019` |
| primaryColor | `#4A90E2` | `#1F76D2` | `#19ff88` |
| secondaryColor | `#00D4FF` | `#0E9AAE` | `#66e6ff` |
| accentColor | `#00FF88` | `#0E8C6A` | `#19ff88` (variant) |
| warningColor | `#FF8C00` | `#B5741A` | `#ffb84a` |
| errorColor | `#FF5F56` | `#CE4038` | `#ff5466` |
| textPrimary | `#E8F4FD` | `#0E1A22` | `#d6dcd8` |
| textSecondary | `#89B4D0` | `#5C6E7E` | `#6c7872` |
| panelColor / panelHeader | `#1E2D42` / `#283C57` | `#FFFFFF` / `#EAF1F7` | `#0d1310` / `#0a0e0c` |

Anche questi vanno ereditati:
- **accent variant** di Darkcodium: phosphor / cyan / amber / red;
- **densità**: compact / regular / comfy, cioè righe 22 / 26 / 30 px, font 11 / 12 / 13 px, header pannello 26 / 30 / 38 px;
- **colori custom** di sfondo e testo sopra il tema;
- **font monospazio**: `Cascadia Mono, Consolas` su Windows, `monospace` altrove (come `Theme::monoFamily`). Il mono va usato per nominativi, frequenze, RST e orari: è ciò che rende leggibile un log a colpo d'occhio.

### 4.2 Proposta: modulo condiviso `decodium-ui`

Oggi il tema esiste in due copie: `DecodiumThemeManager` e `decortty::app::Theme`, quest'ultima con valori costanti duplicati. Con DecoLog diventerebbero tre.

**Proposta:** estrarre un modulo QML/C++ condiviso `decodium-ui`, usato da Decodium, DecoRTTY e DecoLog, contenente:
- `DecodiumThemeManager` (temi, variant, densità, colori custom);
- componenti base: `GlassButton`, `StyledComboBox`, pannello con header, tabella con righe a densità, status bar;
- font e icone comuni.

Si include come sottomodulo git o come libreria CMake. Un cambio di tema in Decodium si riflette ovunque senza copia-incolla.

Regola per DecoLog: **zero colori letterali in QML**. `DecoSyncPanel.qml` in Decodium ha colori scritti a mano: è il caso da non ripetere.

### 4.3 Layout della finestra principale

Stessa grammatica di Decodium: `SplitView` con pannelli ridimensionabili e riposizionabili, dimensioni salvate nel profilo.

```
┌──────────────────────────────────────────────────────────────────┐
│ Barra superiore: profilo stazione · stato Decodium (UDP) · Sync  │
├───────────────┬──────────────────────────────────┬───────────────┤
│ Nuovo QSO /   │ Tabella log                      │ Info call:    │
│ QSO in arrivo │ (filtri, ricerca, colonne)       │ callbook,     │
│ da Decodium   │                                  │ worked-before,│
│               │                                  │ stato QSL     │
├───────────────┴──────────────────────────────────┤ Mappa /       │
│ Pannello inferiore a schede: Award · Statistiche │ Award FT2     │
│ · Upload QSL · Log attività                      │               │
├──────────────────────────────────────────────────┴───────────────┤
│ StatusBar: QSO totali · coda sync · ultimo backup · tema/densità │
└──────────────────────────────────────────────────────────────────┘
```

Le impostazioni vanno in `QSettings` con organizzazione `Decodium` e applicazione `DecoLog`, riusando il meccanismo dei profili attivi di Decodium.

---

## 5. Stack tecnico

### DecoLog Desktop

| Scelta | Motivo |
|---|---|
| C++20, Qt 6, QML, CMake | Identico a Decodium |
| SQLite via Qt SQL, WAL attivo | Offline, un file, facile da salvare |
| qtkeychain | Credenziali QRZ, LoTW, Club Log, cloud |
| Nessun Hamlib nell'MVP | CAT gestito da Decodium |
| Distribuzione: Windows installer, Flatpak, DecodiumOS | Come Decodium |

### DecoLog Cloud (anch'esso da zero)

| Scelta | Motivo |
|---|---|
| Python + FastAPI | API REST con OpenAPI generata, contratto per il client Qt |
| PostgreSQL | Affidabile, JSONB per i campi ADIF extra |
| Web UI server-rendered (template + HTMX) | Nessun secondo progetto frontend |
| Docker Compose su VPS UE | GDPR, deploy semplice |
| Backup notturno off-site + prova di ripristino mensile | |

La Web UI usa gli stessi token colore, esportati come variabili CSS generate da `decodium-ui`.

---

## 6. Modello dati (sintesi)

Schema completo in `db/schema.sql` (incorporato come risorsa e applicato alla prima apertura). Principi:

1. **Nomi ADIF** per tutte le colonne principali.
2. **Lossless:** ogni campo ADIF non mappato finisce in `adif_extra` (JSON). Un import seguito da un export restituisce lo stesso contenuto.
3. **Pronto per il sync dal primo giorno:** `uuid`, `revision`, `updated_at`, `deleted` (soft delete) e `dirty` su ogni QSO. Aggiungerli dopo costa molto di più.
4. **Stato QSL in una tabella separata** (una riga per servizio), invece di decine di colonne.
5. **Orari sempre UTC**, frequenza in MHz con 6 decimali.

---

## 7. Sync (linee guida, dettaglio in Fase 3)

- Ogni modifica locale: `revision += 1`, `dirty = 1`.
- Push: il client invia i QSO `dirty` con la revision che conosceva. Se il server ha una revision più alta, c'è un conflitto: vince l'ultima modifica, ma la versione perdente viene salvata nello storico.
- Pull: `GET /v1/qso?since=<cursor>`, cursore opaco restituito dal server.
- Dedup: stesso call + banda + modo/submode + orario entro ±2 minuti (±10 min per QSO manuali).
- L'UUID è generato dal client, quindi i retry sono idempotenti.

---

## 8. Checklist Fase 0

- [x] Studio funzionale QLog (catalogo §2)
- [x] Analisi tema e layout Decodium (§4)
- [x] Punto d'integrazione UDP identificato (§3)
- [x] Codifica FT2 confermata (`MFSK` / `FT2`)
- [x] Bozza schema database (`db/schema.sql`)
- [x] `decodium-ui` avviato subito, dentro il repo in `libs/decodium-ui` (tema a tre temi + variant + densità + colori custom, componenti base): pronto a diventare sottomodulo condiviso
- [ ] Mockup della finestra principale nei tre temi (la finestra reale esiste già: vedi §4.3, manca il confronto dei tre temi)
- [~] Repo `decolog`: struttura CMake, test e workflow CI Windows/Linux pronti in locale — **manca la creazione su GitHub e il primo push**
- [x] Prototipo minimo: ricezione `LoggedADIF` → scrittura SQLite → riga in tabella (verificato con `decolog_udpsend`)
- [ ] Aggiornare il planning per Paul (niente fork QLog, FT2 ADIF già risolto)

## 9. Decisioni aperte

1. **`decodium-ui` subito o dopo?** Subito costa circa una settimana in più, ma evita di rifare il tema due volte. Consiglio: subito.
2. **Scope award MVP:** solo FT2 Award + DXCC, o anche WAS/WAZ?
3. **Coinvolgere 9H1SR** per le build macOS/Linux, come per Decodium?
