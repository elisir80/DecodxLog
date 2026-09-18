"""DecoLog Cloud — la propagazione, come la scheda del programma.

Gli stessi numeri del pannello Propagazione di DecoLog: SFI, macchie, indice A e
K, aurora, raggi X, campo geomagnetico, rumore, vento solare, e le condizioni
banda per banda di giorno e di notte. La fonte e' la stessa — il XML di N0NBH
(hamqsl.com) — e il modo di leggerlo e' quello di `src/core/Solar.cpp`,
portato qui riga per riga, nomi dei campi compresi (la fonte scrive
"electonflux": si accettano tutti e due).

Il servizio lo chiede una volta all'ora e tiene l'ultimo dato buono: se la fonte
non risponde si mostra quello di prima, con la sua ora, invece di una pagina
vuota. Una stazione non resta senza propagazione perche' un sito e' giu'.
"""

from __future__ import annotations

import datetime as dt
import urllib.request
import xml.etree.ElementTree as ET

URL = "https://www.hamqsl.com/solarxml.php"

# Quanto tiene il dato prima di richiederlo: la fonte si aggiorna ogni ora.
FRESH_FOR = dt.timedelta(hours=1)

_cache: dict = {"data": None, "at": None}


def condition_class(condition: str) -> str:
    """Il colore che merita una condizione, come in solar::conditionClass."""
    c = (condition or "").strip().lower()
    if c.startswith("good") or "open" in c:
        return "good"
    if c.startswith("fair"):
        return "fair"
    if c.startswith("poor"):
        return "poor"
    if "closed" in c:
        return "closed"
    return "unknown"


def _int(text: str | None) -> int:
    try:
        return int((text or "").strip())
    except ValueError:
        return 0


def parse(xml: bytes) -> dict:
    """Legge il XML della fonte. Torna un dato non valido se non si capisce."""
    out: dict = {"valid": False, "hf": [], "vhf": []}
    try:
        root = ET.fromstring(xml)
    except ET.ParseError:
        return out

    def text_of(tag: str) -> str:
        node = root.find(".//" + tag)
        return (node.text or "").strip() if node is not None and node.text else ""

    out.update({
        "source": text_of("source"),
        "updated": text_of("updated"),
        "solarFlux": _int(text_of("solarflux")),
        "aIndex": _int(text_of("aindex")),
        "kIndex": _int(text_of("kindex")),
        "sunspots": _int(text_of("sunspots")),
        "aurora": _int(text_of("aurora")),
        "xray": text_of("xray"),
        "geomagField": text_of("geomagfield"),
        "signalNoise": text_of("signalnoise"),
        "solarWind": text_of("solarwind"),
        "magneticField": text_of("magneticfield"),
        "muf": text_of("muf"),
        "protonFlux": text_of("protonflux"),
        # La fonte scrive "electonflux": si accettano tutti e due.
        "electronFlux": text_of("electonflux") or text_of("electronflux"),
    })

    for band in root.findall(".//calculatedconditions/band"):
        name = (band.get("name") or "").strip()
        if name:
            condition = (band.text or "").strip()
            out["hf"].append({"band": name, "when": (band.get("time") or "").strip(),
                              "condition": condition, "class": condition_class(condition)})

    for phenomenon in root.findall(".//calculatedvhfconditions/phenomenon"):
        name = (phenomenon.get("name") or "").strip()
        if name:
            condition = (phenomenon.text or "").strip()
            out["vhf"].append({"band": name, "when": (phenomenon.get("location") or "").strip(),
                               "condition": condition, "class": condition_class(condition)})

    out["valid"] = bool(out["solarFlux"] or out["sunspots"] or out["hf"])
    return out


def current(fetcher=None) -> dict:
    """L'ultimo dato buono, chiedendolo alla fonte non piu' di una volta all'ora."""
    now = dt.datetime.now(dt.UTC)
    fresh = _cache["at"] is not None and now - _cache["at"] < FRESH_FOR
    if fresh and _cache["data"]:
        return dict(_cache["data"], fetchedAt=_cache["at"].strftime("%Y-%m-%d %H:%M"))

    try:
        raw = (fetcher or _download)()
        data = parse(raw)
    except Exception:       # noqa: BLE001 — la rete non e' mai fatale
        data = {"valid": False, "hf": [], "vhf": []}

    if data.get("valid"):
        _cache["data"] = data
        _cache["at"] = now
        return dict(data, fetchedAt=now.strftime("%Y-%m-%d %H:%M"))

    # Niente di nuovo: si tiene quello di prima, dicendo di quando e'.
    if _cache["data"]:
        when = _cache["at"].strftime("%Y-%m-%d %H:%M") if _cache["at"] else ""
        return dict(_cache["data"], fetchedAt=when, stale=True)
    return data


def _download() -> bytes:
    request = urllib.request.Request(URL, headers={"User-Agent": "DecoLog Cloud"})
    with urllib.request.urlopen(request, timeout=8) as reply:
        return reply.read()


def reset_cache() -> None:
    """Per le prove: si riparte senza ricordi."""
    _cache["data"] = None
    _cache["at"] = None
