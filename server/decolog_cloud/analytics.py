"""DecoLog Cloud — statistiche, diplomi e QSL, calcolati dal log.

Il Cloud non tiene tabelle di riepilogo: i numeri si rifanno dai QSO, esattamente
come fa DecoLog sul computer (`src/core/Awards.cpp`, `LogDatabase::countBy*`).
Cosi' una correzione a un QSO si vede subito ovunque, e le due facce del log —
il programma e la pagina — dicono la stessa cosa.

Le regole sono quelle del programma, portate qui riga per riga: i gruppi di
modi, il prefisso WPX di CQ, i cinquanta stati, le conferme che contano.
"""

from __future__ import annotations

import datetime as dt
from collections import Counter, defaultdict
from dataclasses import dataclass, field

# L'ordine delle bande e' quello di `bands::all()`: dalle lunghe alle corte.
BAND_ORDER = [
    "2190m", "630m", "560m", "160m", "80m", "60m", "40m", "30m", "20m", "17m",
    "15m", "12m", "10m", "8m", "6m", "5m", "4m", "2m", "1.25m", "70cm", "33cm",
    "23cm", "13cm", "9cm", "6cm", "3cm", "1.25cm", "6mm", "4mm", "2.5mm", "2mm",
    "1mm", "submm",
]

PHONE_MODES = {"SSB", "AM", "FM", "DIGITALVOICE", "DSTAR"}

# Gli stati USA per il WAS, con il nome esteso.
US_STATES = {
    "AL": "Alabama", "AK": "Alaska", "AZ": "Arizona", "AR": "Arkansas", "CA": "California",
    "CO": "Colorado", "CT": "Connecticut", "DE": "Delaware", "FL": "Florida", "GA": "Georgia",
    "HI": "Hawaii", "ID": "Idaho", "IL": "Illinois", "IN": "Indiana", "IA": "Iowa",
    "KS": "Kansas", "KY": "Kentucky", "LA": "Louisiana", "ME": "Maine", "MD": "Maryland",
    "MA": "Massachusetts", "MI": "Michigan", "MN": "Minnesota", "MS": "Mississippi",
    "MO": "Missouri", "MT": "Montana", "NE": "Nebraska", "NV": "Nevada", "NH": "New Hampshire",
    "NJ": "New Jersey", "NM": "New Mexico", "NY": "New York", "NC": "North Carolina",
    "ND": "North Dakota", "OH": "Ohio", "OK": "Oklahoma", "OR": "Oregon", "PA": "Pennsylvania",
    "RI": "Rhode Island", "SC": "South Carolina", "SD": "South Dakota", "TN": "Tennessee",
    "TX": "Texas", "UT": "Utah", "VT": "Vermont", "VA": "Virginia", "WA": "Washington",
    "WV": "West Virginia", "WI": "Wisconsin", "WY": "Wyoming",
}

USA_ENTITIES = {291, 6, 110}   # USA, Alaska, Hawaii

# I servizi QSL come li conosce DecoLog, con i campi ADIF che li dicono.
QSL_SERVICES = [
    ("lotw", "LoTW", "LOTW_QSL_SENT", "LOTW_QSL_RCVD"),
    ("card", "Cartolina", "QSL_SENT", "QSL_RCVD"),
    ("eqsl", "eQSL", "EQSL_QSL_SENT", "EQSL_QSL_RCVD"),
    ("qrz", "QRZ Logbook", "QRZCOM_QSO_UPLOAD_STATUS", "QRZCOM_QSO_DOWNLOAD_STATUS"),
    ("clublog", "Club Log", "CLUBLOG_QSO_UPLOAD_STATUS", ""),
]

# I continenti, nell'ordine in cui si e' abituati a vederli.
CONTINENTS = ["EU", "NA", "SA", "AS", "AF", "OC", "AN"]

# I sei continenti del WAC col loro nome: l'Antartide entra nell'elenco ma non
# nel traguardo, come in Awards.cpp.
CONTINENT_NAMES = {
    "EU": "Europa", "NA": "Nord America", "SA": "Sud America", "AS": "Asia",
    "AF": "Africa", "OC": "Oceania", "AN": "Antartide",
}

JAPAN = 339

# Le 47 prefetture, coi numeri di ADIF: gli stessi di Awards.cpp.
JAPAN_PREFECTURES = {
    "01": "Hokkaido", "02": "Aomori", "03": "Iwate", "04": "Akita", "05": "Yamagata",
    "06": "Miyagi", "07": "Fukushima", "08": "Niigata", "09": "Nagano", "10": "Tokyo",
    "11": "Kanagawa", "12": "Chiba", "13": "Saitama", "14": "Ibaraki", "15": "Tochigi",
    "16": "Gunma", "17": "Yamanashi", "18": "Shizuoka", "19": "Gifu", "20": "Aichi",
    "21": "Mie", "22": "Kyoto", "23": "Shiga", "24": "Nara", "25": "Osaka",
    "26": "Wakayama", "27": "Hyogo", "28": "Toyama", "29": "Fukui", "30": "Ishikawa",
    "31": "Okayama", "32": "Shimane", "33": "Yamaguchi", "34": "Tottori", "35": "Hiroshima",
    "36": "Kagawa", "37": "Tokushima", "38": "Ehime", "39": "Kochi", "40": "Fukuoka",
    "41": "Saga", "42": "Nagasaki", "43": "Kumamoto", "44": "Oita", "45": "Miyazaki",
    "46": "Kagoshima", "47": "Okinawa",
}

_IGNORED_SUFFIXES = {"P", "M", "MM", "AM", "QRP", "QRPP", "A", "B", "LH", "J", "R", "T"}


# ── Il QSO come lo guardano i conti ───────────────────────────────────────────


@dataclass
class Row:
    """Un QSO ridotto a quello che serve ai conti, letto una volta sola."""

    call: str = ""
    band: str = ""
    mode: str = ""
    submode: str = ""
    when: dt.datetime | None = None
    dxcc: int = 0
    country: str = ""
    continent: str = ""
    cqz: int = 0
    state: str = ""
    county: str = ""
    grid: str = ""
    iota: str = ""
    pota: str = ""
    sota: str = ""
    wwff: str = ""
    confirmed_lotw: bool = False
    confirmed_card: bool = False
    confirmed_eqsl: bool = False
    fields: dict = field(default_factory=dict)

    @property
    def label_mode(self) -> str:
        # Il sottomodo dice FT2 dove il modo direbbe solo MFSK; in SSB il
        # sottomodo (LSB/USB) non aggiunge niente.
        if self.submode and self.mode != "SSB":
            return self.submode
        return self.mode


def _int(value) -> int:
    try:
        return int(str(value).strip())
    except (TypeError, ValueError):
        return 0


def _when(qso) -> dt.datetime | None:
    """L'ora del QSO: prima quella normalizzata, poi i campi ADIF."""
    raw = getattr(qso, "started_at", None)
    if isinstance(raw, dt.datetime):
        return raw if raw.tzinfo else raw.replace(tzinfo=dt.UTC)
    fields = getattr(qso, "fields", None) or {}
    date = str(fields.get("QSO_DATE") or "")
    time = str(fields.get("TIME_ON") or "000000")
    if len(date) < 8:
        return None
    time = (time + "000000")[:6]
    try:
        return dt.datetime(int(date[:4]), int(date[4:6]), int(date[6:8]),
                           int(time[:2]), int(time[2:4]), int(time[4:6]), tzinfo=dt.UTC)
    except ValueError:
        return None


def row_of(qso) -> Row:
    f = {k.upper(): v for k, v in (getattr(qso, "fields", None) or {}).items()}

    def text(*names: str) -> str:
        for name in names:
            value = f.get(name)
            if value not in (None, ""):
                return str(value).strip()
        return ""

    def is_yes(name: str) -> bool:
        return text(name).upper().startswith("Y")

    return Row(
        call=(getattr(qso, "call", "") or text("CALL")).upper(),
        band=(getattr(qso, "band", "") or text("BAND")).lower(),
        mode=(getattr(qso, "mode", "") or text("MODE")).upper(),
        submode=(getattr(qso, "submode", "") or text("SUBMODE")).upper(),
        when=_when(qso),
        dxcc=_int(text("DXCC")),
        country=text("COUNTRY"),
        continent=text("CONT").upper(),
        cqz=_int(text("CQZ")),
        state=text("STATE").upper(),
        county=text("CNTY").upper(),
        grid=text("GRIDSQUARE").upper(),
        iota=text("IOTA").upper(),
        pota=text("POTA_REF", "MY_POTA_REF").upper(),
        sota=text("SOTA_REF").upper(),
        wwff=text("WWFF_REF").upper(),
        confirmed_lotw=is_yes("LOTW_QSL_RCVD"),
        confirmed_card=is_yes("QSL_RCVD"),
        confirmed_eqsl=is_yes("EQSL_QSL_RCVD"),
        fields=f,
    )


def mode_matches(group: str, row: Row) -> bool:
    """Gli stessi gruppi di `modeMatches` in Awards.cpp."""
    if not group:
        return True
    if group == "FT2":
        return row.submode == "FT2"
    if group == "FT8":
        return row.mode == "FT8"
    if group == "CW":
        return row.mode == "CW"
    if group == "PHONE":
        return row.mode in PHONE_MODES
    if group == "DIGITAL":
        return row.mode != "CW" and row.mode not in PHONE_MODES
    return True


def japan_prefecture(state: str) -> str:
    """La prefettura da come la scrivono i log: "12", "JA12", "12 Chiba"."""
    digits = ""
    for c in state.strip().upper():
        if c.isdigit():
            digits += c
        elif digits:
            break
    if not digits:
        return ""
    number = int(digits)
    return "%02d" % number if 1 <= number <= 47 else ""


def japan_district(callsign: str) -> str:
    """Il distretto (AJD): la cifra fra prefisso e suffisso, JA1AA -> 1."""
    call = callsign.strip().upper()
    base = call
    for part in call.split("/"):
        if len(part) >= 3:
            base = part
            break
    for i, c in enumerate(base):
        if c.isdigit() and i + 1 < len(base) and base[i + 1:].isalpha():
            return c
    return ""


def japan_jarl_code(county: str) -> str:
    """Il numero JARL del campo CNTY: quattro o sei cifre la citta', cinque il gun."""
    digits = "".join(c for c in county if c.isdigit())
    if not 4 <= len(digits) <= 6:
        return ""
    return digits if 1 <= int(digits[:2]) <= 47 else ""


def wpx_prefix(callsign: str) -> str:
    """Il prefisso WPX secondo CQ: N8BJQ -> N8, EA8/OH2XX -> EA8, W1AW/4 -> W4."""
    call = (callsign or "").strip().upper()
    if not call:
        return ""
    parts = [p for p in call.split("/") if p and p not in _IGNORED_SUFFIXES]
    if not parts:
        return ""

    area = ""
    if len(parts) >= 2 and len(parts[-1]) == 1 and parts[-1].isdigit():
        area = parts.pop()

    if len(parts) >= 2:
        # La parte piu' corta e' il prefisso; senza cifre si aggiunge lo zero.
        prefix = parts[0] if len(parts[0]) <= len(parts[1]) else parts[1]
        if not any(c.isdigit() for c in prefix):
            prefix += "0"
    else:
        prefix = _base_prefix(parts[0])

    if area:
        for i in range(len(prefix) - 1, -1, -1):
            if prefix[i].isdigit():
                prefix = prefix[:i] + area
                break
    return prefix


def _base_prefix(call: str) -> str:
    for i in range(len(call) - 1, -1, -1):
        if call[i].isdigit() and i + 1 < len(call) and call[i + 1:].isalpha():
            return call[: i + 1]
    return call[:2] + "0"


# ── Statistiche ───────────────────────────────────────────────────────────────


def _sorted_bands(counter: Counter) -> list[tuple[str, int]]:
    def where(band: str) -> int:
        return BAND_ORDER.index(band) if band in BAND_ORDER else len(BAND_ORDER)

    return sorted(counter.items(), key=lambda kv: where(kv[0]))


def statistics(rows: list[Row], mode_group: str = "", year: int = 0, band: str = "") -> dict:
    """Gli stessi riquadri della finestra Statistiche di DecoLog."""
    kept = [r for r in rows
            if mode_matches(mode_group, r)
            and (not band or r.band == band)
            and (not year or (r.when and r.when.year == year))]

    by_year: Counter = Counter()
    by_hour: Counter = Counter()
    by_band: Counter = Counter()
    by_mode: Counter = Counter()
    by_continent: Counter = Counter()
    by_month: Counter = Counter()
    heat: dict[str, Counter] = defaultdict(Counter)
    calls: set[str] = set()
    entities: set[int] = set()
    grids: set[str] = set()
    first = last = None

    for r in kept:
        if r.call:
            calls.add(r.call)
        if r.dxcc:
            entities.add(r.dxcc)
        if len(r.grid) >= 4:
            grids.add(r.grid[:4])
        if r.band:
            by_band[r.band] += 1
        if r.label_mode:
            by_mode[r.label_mode] += 1
        if r.continent:
            by_continent[r.continent] += 1
        if r.when:
            by_year[r.when.year] += 1
            by_hour[r.when.hour] += 1
            by_month[f"{r.when.year}-{r.when.month:02d}"] += 1
            if r.band:
                heat[r.band][r.when.hour] += 1
            first = r.when if first is None or r.when < first else first
            last = r.when if last is None or r.when > last else last

    bands_used = [b for b, _ in _sorted_bands(by_band)]
    busiest = max((n for c in heat.values() for n in c.values()), default=0)

    return {
        "qsos": len(kept),
        "calls": len(calls),
        "entities": len(entities),
        "grids": len(grids),
        "first": first,
        "last": last,
        "years": sorted(by_year.items()),
        "months": sorted(by_month.items())[-24:],
        "hours": [(h, by_hour.get(h, 0)) for h in range(24)],
        "bands": _sorted_bands(by_band),
        "modes": by_mode.most_common(),
        "continents": [(c, by_continent.get(c, 0)) for c in CONTINENTS if by_continent.get(c)],
        # La mappa di calore: per ogni banda usata, i 24 valori dell'ora UTC.
        "heat": [{"band": b, "hours": [heat[b].get(h, 0) for h in range(24)]} for b in bands_used],
        "heat_max": busiest,
        "all_years": sorted({y for y, _ in by_year.items()}, reverse=True),
    }


# ── Diplomi ───────────────────────────────────────────────────────────────────


@dataclass
class AwardItem:
    key: str
    name: str = ""
    worked: set = field(default_factory=set)
    confirmed: set = field(default_factory=set)
    qsos: int = 0
    first_call: str = ""
    first: dt.datetime | None = None

    @property
    def is_confirmed(self) -> bool:
        return bool(self.confirmed)


@dataclass
class Award:
    id: str
    title: str
    target: int = 0
    total: int = 0
    items: list = field(default_factory=list)

    @property
    def worked(self) -> int:
        return len(self.items)

    @property
    def confirmed(self) -> int:
        return sum(1 for i in self.items if i.is_confirmed)

    def band_totals(self, bands: list[str]) -> list[dict]:
        out = []
        for band in bands:
            out.append({
                "band": band,
                "worked": sum(1 for i in self.items if band in i.worked),
                "confirmed": sum(1 for i in self.items if band in i.confirmed),
            })
        return out

    @property
    def slots(self) -> int:
        """I band slot: la somma dei confermati banda per banda (DXCC Challenge)."""
        return sum(len(i.confirmed) for i in self.items)


AWARD_DEFS = [
    ("dxcc", "DXCC", 100, 340),
    ("ft2", "FT2 Award", 100, 0),
    ("wac", "WAC", 6, 6),
    # Quante siano le entita' africane lo dice il cty.csv, che qui non c'e':
    # il Cloud conta quelle lavorate e lascia il traguardo al programma.
    ("waac", "WAAC", 0, 0),
    ("waz", "WAZ", 40, 40),
    ("was", "WAS", 50, 50),
    ("waja", "WAJA", 47, 47),
    ("ajd", "AJD", 10, 10),
    ("jcc", "JCC", 100, 0),
    ("jcg", "JCG", 100, 0),
    ("wpx", "WPX", 300, 0),
    ("grids", "Locatori", 100, 0),
    ("iota", "IOTA", 100, 0),
    ("pota", "POTA", 0, 0),
    ("sota", "SOTA", 0, 0),
    ("wwff", "WWFF", 44, 0),
]


def _is_iota(ref: str) -> bool:
    return (len(ref) == 6 and ref[:2] in {"AF", "AN", "AS", "EU", "NA", "OC", "SA"}
            and ref[2] == "-" and ref[3:].isdigit())


def awards(rows: list[Row], band: str = "", mode_group: str = "",
           confirm_lotw: bool = True, confirm_card: bool = True,
           confirm_eqsl: bool = False) -> list[Award]:
    """Tutti i diplomi in una passata sola, con le regole di Awards.cpp."""
    built: dict[str, dict[str, AwardItem]] = {a[0]: {} for a in AWARD_DEFS}

    for r in rows:
        if band and r.band != band:
            continue
        if not mode_matches(mode_group, r):
            continue
        confirmed = ((confirm_lotw and r.confirmed_lotw)
                     or (confirm_card and r.confirmed_card)
                     or (confirm_eqsl and r.confirmed_eqsl))

        def add(award_id: str, key: str, name: str = "") -> None:
            if not key:
                return
            item = built[award_id].get(key)
            if item is None:
                item = AwardItem(key=key, name=name, first_call=r.call, first=r.when)
                built[award_id][key] = item
            item.qsos += 1
            if r.band:
                item.worked.add(r.band)
                if confirmed:
                    item.confirmed.add(r.band)

        if r.dxcc:
            add("dxcc", str(r.dxcc), r.country)
            if r.submode == "FT2":
                add("ft2", str(r.dxcc), r.country)
        if r.continent in CONTINENT_NAMES:
            add("wac", r.continent, CONTINENT_NAMES[r.continent])
        if r.continent == "AF" and r.dxcc:
            add("waac", str(r.dxcc), r.country)
        if 1 <= r.cqz <= 40:
            add("waz", str(r.cqz))
        if r.dxcc in USA_ENTITIES and r.state in US_STATES:
            add("was", r.state, US_STATES[r.state])
        if r.dxcc == JAPAN:
            prefecture = japan_prefecture(r.state)
            if prefecture:
                add("waja", prefecture, JAPAN_PREFECTURES[prefecture])
            district = japan_district(r.call)
            if district:
                add("ajd", district)
            jarl = japan_jarl_code(r.county)
            if jarl:
                # Nel web si legge il nome: per una citta' il nome e' il numero,
                # con la prefettura accanto perche' dica qualcosa.
                prefecture = JAPAN_PREFECTURES.get(jarl[:2], "")
                add("jcg" if len(jarl) == 5 else "jcc", jarl,
                    (jarl + " " + prefecture).strip())
        add("wpx", wpx_prefix(r.call))
        if len(r.grid) >= 4:
            add("grids", r.grid[:4])
        if _is_iota(r.iota):
            add("iota", r.iota)
        add("pota", r.pota)
        add("sota", r.sota)
        add("wwff", r.wwff)

    def order(item: AwardItem):
        return (0, int(item.key), "") if item.key.isdigit() else (1, 0, item.key)

    out = []
    for award_id, title, target, total in AWARD_DEFS:
        items = sorted(built[award_id].values(), key=order)
        out.append(Award(id=award_id, title=title, target=target, total=total, items=items))
    return out


# I moltiplicatori che i contest usano davvero: le entita' DXCC, i prefissi
# alla WPX e le zone CQ. Quale conta dipende dal contest, quindi si contano
# tutti e tre e si lascia scegliere.
CONTEST_MULTIPLIERS = [
    ("dxcc", "Entita' DXCC"),
    ("wpx", "Prefissi (WPX)"),
    ("cqz", "Zone CQ"),
]


def contest_score(rows: list[Row], since: dt.datetime | None = None,
                  until: dt.datetime | None = None, mode_group: str = "",
                  points_per_qso: int = 1, multiplier: str = "dxcc") -> dict:
    """Il punteggio di una sessione: QSO validi, punti, moltiplicatori, totale.

    Si conta come si conta in gara: i duplicati (stesso nominativo, stessa
    banda, stesso gruppo di modi) non valgono, i moltiplicatori si contano una
    volta per banda, e il totale e' punti per moltiplicatori. Le regole vere
    cambiano da contest a contest: qui si tengono quelle che valgono quasi
    sempre, e il resto lo dice l'occhio di chi opera.
    """
    seen: set[tuple[str, str, str]] = set()
    per_band: dict[str, dict] = {}
    mults: set[tuple[str, str]] = set()
    valid = 0
    dupes = 0
    points = 0

    for r in rows:
        if r.when is None:
            continue
        if since is not None and r.when < since:
            continue
        if until is not None and r.when > until:
            continue
        if not mode_matches(mode_group, r):
            continue

        group = "CW" if r.mode == "CW" else ("PHONE" if r.mode in PHONE_MODES else "DIGI")
        key = (r.call, r.band, group)
        band = per_band.setdefault(r.band or "?", {"band": r.band or "?", "qsos": 0, "dupes": 0,
                                                   "points": 0, "mults": 0})
        if key in seen:
            dupes += 1
            band["dupes"] += 1
            continue
        seen.add(key)
        valid += 1
        points += points_per_qso
        band["qsos"] += 1
        band["points"] += points_per_qso

        if multiplier == "wpx":
            token = wpx_prefix(r.call)
        elif multiplier == "cqz":
            token = str(r.cqz) if 1 <= r.cqz <= 40 else ""
        else:
            token = str(r.dxcc) if r.dxcc else ""
        if token:
            before = len(mults)
            mults.add((r.band, token))
            if len(mults) > before:
                band["mults"] += 1

    bands = sorted(per_band.values(), key=lambda b: BAND_ORDER.index(b["band"]) if b["band"] in BAND_ORDER else 99)
    return {
        "qsos": valid,
        "dupes": dupes,
        "points": points,
        "multipliers": len(mults),
        "score": points * max(1, len(mults)) if mults else points,
        "bands": bands,
        "multiplier": multiplier,
        "points_per_qso": points_per_qso,
    }


def missing_states(award: Award) -> list[tuple[str, str]]:
    """Gli stati che mancano al WAS: il diploma si chiude sapendo cosa cercare."""
    done = {i.key for i in award.items}
    return [(code, name) for code, name in sorted(US_STATES.items()) if code not in done]


def missing_zones(award: Award) -> list[int]:
    done = {int(i.key) for i in award.items if i.key.isdigit()}
    return [z for z in range(1, 41) if z not in done]


# ── QSL ───────────────────────────────────────────────────────────────────────


def qsl_summary(rows: list[Row]) -> list[dict]:
    """Servizio per servizio: da mandare, inviate, confermate.

    Le stesse colonne della scheda "Invio QSL" del programma. "Da mandare" sono
    i QSO che quel servizio non ha ancora visto.
    """
    out = []
    for service_id, label, sent_field, rcvd_field in QSL_SERVICES:
        sent = rcvd = pending = 0
        for r in rows:
            gone = bool(sent_field) and str(r.fields.get(sent_field, "")).upper().startswith("Y")
            if gone:
                sent += 1
            else:
                pending += 1
            if rcvd_field and str(r.fields.get(rcvd_field, "")).upper().startswith("Y"):
                rcvd += 1
        out.append({"id": service_id, "label": label, "sent": sent, "rcvd": rcvd,
                    "pending": pending})
    return out


# Come il programma scrive la via di una QSL di carta (`QSL_SENT_VIA`).
QSL_VIA = {"B": "bureau", "D": "diretta", "E": "elettronica", "M": "manager"}


def paper_queue(rows: list[Row]) -> dict:
    """La coda delle QSL di carta: da mandare, mandate, ricevute, e per che via.

    E' la finestra "QSL di carta" del programma, vista dal Cloud: i campi sono
    quelli ADIF che il log porta con se' (`QSL_SENT`, `QSL_RCVD`,
    `QSL_SENT_VIA`, `QSL_QUEUE` per chi la mette in coda a mano).
    """
    to_send: list[dict] = []
    sent = rcvd = 0
    vias: Counter = Counter()

    for r in rows:
        state = str(r.fields.get("QSL_SENT", "")).upper()
        received = str(r.fields.get("QSL_RCVD", "")).upper().startswith("Y")
        via = str(r.fields.get("QSL_SENT_VIA", "")).upper()[:1]
        if received:
            rcvd += 1
        if state.startswith("Y"):
            sent += 1
            if via:
                vias[QSL_VIA.get(via, via)] += 1
            continue
        # "Q" e' la coda di ADIF: il QSO aspetta la cartolina.
        if state.startswith("Q") or str(r.fields.get("QSL_QUEUE", "")).upper().startswith("Y"):
            to_send.append({
                "call": r.call, "band": r.band, "mode": r.label_mode,
                "when": r.when.strftime("%Y-%m-%d") if r.when else "",
                "via": QSL_VIA.get(via, via) if via else "",
                "country": r.country,
            })

    to_send.sort(key=lambda q: q["when"], reverse=True)
    return {"queue": to_send, "sent": sent, "rcvd": rcvd,
            "vias": sorted(vias.items(), key=lambda kv: -kv[1])}


def grid_points(rows: list[Row]) -> list[dict]:
    """I locatori lavorati, con il centro in gradi: la mappa li disegna li'."""
    seen: dict[str, dict] = {}
    for r in rows:
        grid = r.grid[:4]
        if len(grid) < 4 or not grid[:2].isalpha() or not grid[2:4].isdigit():
            continue
        entry = seen.get(grid)
        if entry is None:
            lon = (ord(grid[0]) - 65) * 20 + int(grid[2]) * 2 - 180 + 1
            lat = (ord(grid[1]) - 65) * 10 + int(grid[3]) - 90 + 0.5
            entry = {"grid": grid, "lon": lon, "lat": lat, "qsos": 0, "confirmed": False}
            seen[grid] = entry
        entry["qsos"] += 1
        if r.confirmed_lotw or r.confirmed_card or r.confirmed_eqsl:
            entry["confirmed"] = True
    return sorted(seen.values(), key=lambda e: e["grid"])
