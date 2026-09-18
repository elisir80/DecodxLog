"""DecoLog Cloud — i conti del log: statistiche, diplomi, QSL.

Le regole sono quelle del programma (`src/core/Awards.cpp`, le countBy* di
LogDatabase). Queste prove guardano che qui diano gli stessi risultati: un
diploma che sul computer e' confermato non puo' risultare solo lavorato sulla
pagina.
"""

from __future__ import annotations

import datetime as dt

from decolog_cloud import analytics


class FakeQso:
    """Un QSO come lo tiene il server: le colonne comode piu' i campi ADIF."""

    def __init__(self, **fields):
        self.fields = fields
        self.call = fields.get("CALL", "")
        self.band = fields.get("BAND", "")
        self.mode = fields.get("MODE", "")
        self.submode = fields.get("SUBMODE", "")
        self.started_at = None


def qso(call="DL9ZZT", band="20m", mode="MFSK", submode="FT2", date="20260918",
        time="070000", **extra):
    return analytics.row_of(FakeQso(CALL=call, BAND=band, MODE=mode, SUBMODE=submode,
                                    QSO_DATE=date, TIME_ON=time, **extra))


# ── Il prefisso WPX, con gli esempi del regolamento CQ ────────────────────────


def test_wpx_prefix_follows_the_cq_rules():
    assert analytics.wpx_prefix("N8BJQ") == "N8"
    assert analytics.wpx_prefix("2E0ABC") == "2E0"
    assert analytics.wpx_prefix("EA8/OH2XX") == "EA8"
    assert analytics.wpx_prefix("W1AW/4") == "W4"
    assert analytics.wpx_prefix("LX/DL1ABC") == "LX0"
    # I suffissi che non contano non cambiano il prefisso.
    assert analytics.wpx_prefix("IK2ABC/P") == "IK2"
    assert analytics.wpx_prefix("") == ""


# ── Gruppi di modi ────────────────────────────────────────────────────────────


def test_mode_groups_are_the_ones_of_the_program():
    ft2 = qso(mode="MFSK", submode="FT2")
    cw = qso(mode="CW", submode="")
    ssb = qso(mode="SSB", submode="USB")
    ft8 = qso(mode="FT8", submode="")

    assert analytics.mode_matches("FT2", ft2) and not analytics.mode_matches("FT2", ft8)
    assert analytics.mode_matches("CW", cw) and not analytics.mode_matches("CW", ssb)
    assert analytics.mode_matches("PHONE", ssb) and not analytics.mode_matches("PHONE", cw)
    # Digitale e' "ne' CW ne' fonia": FT2 e FT8 ci stanno, l'SSB no.
    assert analytics.mode_matches("DIGITAL", ft2)
    assert not analytics.mode_matches("DIGITAL", ssb)
    # Senza gruppo passa tutto.
    assert analytics.mode_matches("", ssb)


def test_the_mode_shown_is_the_submode_except_in_ssb():
    assert qso(mode="MFSK", submode="FT2").label_mode == "FT2"
    assert qso(mode="SSB", submode="USB").label_mode == "SSB"


# ── Statistiche ───────────────────────────────────────────────────────────────


def test_statistics_count_what_the_window_shows():
    rows = [
        qso(call="DL9ZZT", band="20m", date="20260918", time="070000", DXCC="230", CONT="EU",
            GRIDSQUARE="JO62"),
        qso(call="DL9ZZT", band="40m", date="20260918", time="070000", DXCC="230", CONT="EU",
            GRIDSQUARE="JO62"),
        qso(call="EA5XYZ", band="20m", date="20250101", time="230000", DXCC="281", CONT="EU",
            GRIDSQUARE="IM98"),
    ]
    s = analytics.statistics(rows)
    assert s["qsos"] == 3
    assert s["calls"] == 2          # DL9ZZT contato una volta
    assert s["entities"] == 2
    assert s["grids"] == 2
    assert dict(s["bands"])["20m"] == 2
    assert dict(s["years"])[2026] == 2
    assert dict(s["hours"])[7] == 2 and dict(s["hours"])[23] == 1
    assert dict(s["continents"])["EU"] == 3
    # Le bande escono nell'ordine di DecoLog: prima le lunghe.
    assert [b for b, _ in s["bands"]] == ["40m", "20m"]


def test_statistics_filters_by_mode_and_year():
    rows = [qso(mode="MFSK", submode="FT2", date="20260918"),
            qso(mode="CW", submode="", date="20250918")]
    assert analytics.statistics(rows, mode_group="FT2")["qsos"] == 1
    assert analytics.statistics(rows, year=2025)["qsos"] == 1


def test_the_heat_map_says_when_a_band_is_open():
    rows = [qso(band="20m", time="140000"), qso(band="20m", time="140000"),
            qso(band="40m", time="020000")]
    s = analytics.statistics(rows)
    heat = {row["band"]: row["hours"] for row in s["heat"]}
    assert heat["20m"][14] == 2
    assert heat["40m"][2] == 1
    assert s["heat_max"] == 2


# ── Diplomi ───────────────────────────────────────────────────────────────────


def test_awards_count_worked_and_confirmed_per_band():
    rows = [
        qso(call="DL9ZZT", band="20m", DXCC="230", CQZ="14", GRIDSQUARE="JO62",
            LOTW_QSL_RCVD="Y"),
        qso(call="DL9ZZT", band="40m", DXCC="230", CQZ="14", GRIDSQUARE="JO62"),
        qso(call="EA5XYZ", band="20m", DXCC="281", CQZ="14", GRIDSQUARE="IM98"),
    ]
    dxcc = next(a for a in analytics.awards(rows) if a.id == "dxcc")
    assert dxcc.worked == 2
    assert dxcc.confirmed == 1
    totals = {t["band"]: t for t in dxcc.band_totals(["20m", "40m"])}
    assert totals["20m"]["worked"] == 2 and totals["20m"]["confirmed"] == 1
    assert totals["40m"]["worked"] == 1 and totals["40m"]["confirmed"] == 0
    # Una sola zona, lavorata su due bande: un elemento, due band slot lavorati.
    waz = next(a for a in analytics.awards(rows) if a.id == "waz")
    assert waz.worked == 1 and waz.confirmed == 1


def test_which_confirmations_count_is_a_choice():
    rows = [qso(DXCC="230", EQSL_QSL_RCVD="Y")]
    assert next(a for a in analytics.awards(rows) if a.id == "dxcc").confirmed == 0
    with_eqsl = analytics.awards(rows, confirm_eqsl=True)
    assert next(a for a in with_eqsl if a.id == "dxcc").confirmed == 1


def test_ft2_award_only_counts_ft2():
    rows = [qso(mode="MFSK", submode="FT2", DXCC="230"),
            qso(mode="FT8", submode="", DXCC="281")]
    ft2 = next(a for a in analytics.awards(rows) if a.id == "ft2")
    assert ft2.worked == 1


def test_was_only_counts_american_states():
    rows = [qso(call="W1AW", DXCC="291", STATE="CT"),
            qso(call="VE3ABC", DXCC="1", STATE="ON")]   # Canada: non e' WAS
    was = next(a for a in analytics.awards(rows) if a.id == "was")
    assert [i.key for i in was.items] == ["CT"]
    assert ("CT", "Connecticut") not in analytics.missing_states(was)
    assert len(analytics.missing_states(was)) == 49


def test_missing_zones_are_the_ones_not_worked():
    rows = [qso(CQZ="14"), qso(CQZ="15")]
    waz = next(a for a in analytics.awards(rows) if a.id == "waz")
    missing = analytics.missing_zones(waz)
    assert 14 not in missing and 15 not in missing
    assert len(missing) == 38


def test_awards_can_be_asked_band_by_band():
    rows = [qso(band="20m", DXCC="230"), qso(band="40m", DXCC="281")]
    only20 = analytics.awards(rows, band="20m")
    assert next(a for a in only20 if a.id == "dxcc").worked == 1


# ── QSL e mappa ───────────────────────────────────────────────────────────────


def test_qsl_summary_counts_sent_and_received():
    rows = [qso(LOTW_QSL_SENT="Y", LOTW_QSL_RCVD="Y"),
            qso(LOTW_QSL_SENT="Y"),
            qso(QSL_SENT="Y", QSL_RCVD="Y")]
    summary = {s["id"]: s for s in analytics.qsl_summary(rows)}
    assert summary["lotw"]["sent"] == 2 and summary["lotw"]["rcvd"] == 1
    assert summary["card"]["sent"] == 1 and summary["card"]["rcvd"] == 1
    assert summary["eqsl"]["sent"] == 0


def test_grid_points_land_where_the_grid_is():
    rows = [qso(GRIDSQUARE="JN61fx", LOTW_QSL_RCVD="Y"), qso(GRIDSQUARE="JN61aa")]
    points = analytics.grid_points(rows)
    assert len(points) == 1                     # lo stesso quadrato, due QSO
    point = points[0]
    assert point["grid"] == "JN61" and point["qsos"] == 2 and point["confirmed"]
    # JN61 e' l'Italia centrale: est di Greenwich, a nord dell'equatore.
    assert 12 <= point["lon"] <= 14 and 41 <= point["lat"] <= 43

    # Un locatore malscritto non finisce sulla mappa.
    assert analytics.grid_points([qso(GRIDSQUARE="XX")]) == []
