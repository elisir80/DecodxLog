"""DecoDXLog Cloud — la propagazione, letta come la legge il programma.

Il XML e' quello di N0NBH (hamqsl.com) e il modo di leggerlo e' quello di
`src/core/Solar.cpp`. Queste prove usano un pezzo di XML vero: nessuna rete.
"""

from __future__ import annotations

from decolog_cloud import solar

XML = b"""<solar>
  <solardata>
    <source url="http://www.hamqsl.com/solar.html">N0NBH</source>
    <updated>18 Sep 2026 0730 GMT</updated>
    <solarflux>168</solarflux>
    <aindex>7</aindex>
    <kindex>3</kindex>
    <sunspots>142</sunspots>
    <aurora>3</aurora>
    <xray>B7.2</xray>
    <geomagfield>QUIET</geomagfield>
    <signalnoise>S1-S2</signalnoise>
    <solarwind>412.3</solarwind>
    <magneticfield>2.1</magneticfield>
    <muf>28.7 MHz</muf>
    <protonflux>510</protonflux>
    <electonflux>1200</electonflux>
    <calculatedconditions>
      <band name="80m-40m" time="day">Fair</band>
      <band name="80m-40m" time="night">Good</band>
      <band name="30m-20m" time="day">Good</band>
      <band name="30m-20m" time="night">Good</band>
      <band name="17m-15m" time="day">Good</band>
      <band name="17m-15m" time="night">Poor</band>
      <band name="12m-10m" time="day">Fair</band>
      <band name="12m-10m" time="night">Band Closed</band>
    </calculatedconditions>
    <calculatedvhfconditions>
      <phenomenon name="vhf-aurora" location="northern_hemi">Band Closed</phenomenon>
      <phenomenon name="E-Skip" location="europe">50MHz es</phenomenon>
    </calculatedvhfconditions>
  </solardata>
</solar>"""


def test_the_numbers_are_the_ones_of_the_source():
    data = solar.parse(XML)
    assert data["valid"]
    assert data["solarFlux"] == 168
    assert data["sunspots"] == 142
    assert data["aIndex"] == 7 and data["kIndex"] == 3
    assert data["aurora"] == 3
    assert data["xray"] == "B7.2"
    assert data["geomagField"] == "QUIET"
    assert data["muf"] == "28.7 MHz"
    assert data["source"] == "N0NBH"
    assert data["updated"] == "18 Sep 2026 0730 GMT"


def test_the_source_writes_electonflux_and_we_take_it_anyway():
    # La fonte sbaglia il nome del campo da sempre: il programma lo accetta, e
    # anche noi.
    assert solar.parse(XML)["electronFlux"] == "1200"


def test_band_conditions_come_with_day_and_night():
    hf = solar.parse(XML)["hf"]
    assert {"band": "80m-40m", "when": "day", "condition": "Fair", "class": "fair"} in hf
    assert {"band": "12m-10m", "when": "night", "condition": "Band Closed", "class": "closed"} in hf
    vhf = solar.parse(XML)["vhf"]
    assert vhf[0]["band"] == "vhf-aurora" and vhf[0]["when"] == "northern_hemi"


def test_the_condition_class_is_the_one_of_the_program():
    assert solar.condition_class("Good") == "good"
    assert solar.condition_class("Fair") == "fair"
    assert solar.condition_class("Poor") == "poor"
    assert solar.condition_class("Band Closed") == "closed"
    assert solar.condition_class("50MHz es") == "unknown"
    # "open" vale come buono, come in solar::conditionClass.
    assert solar.condition_class("Band Open") == "good"
    assert solar.condition_class("") == "unknown"


def test_rubbish_does_not_become_propagation():
    assert solar.parse(b"non e' xml")["valid"] is False
    assert solar.parse(b"<solar></solar>")["valid"] is False


def test_the_source_is_asked_once_an_hour():
    solar.reset_cache()
    calls = []

    def fetcher():
        calls.append(1)
        return XML

    first = solar.current(fetcher)
    second = solar.current(fetcher)
    assert first["solarFlux"] == 168 and second["solarFlux"] == 168
    assert len(calls) == 1       # la seconda volta si riusa quello di prima
    assert first["fetchedAt"]


def test_a_source_that_is_down_leaves_the_last_good_reading():
    solar.reset_cache()
    solar.current(lambda: XML)
    solar._cache["at"] = None    # come se fosse passata un'ora

    def broken():
        raise OSError("la fonte non risponde")

    data = solar.current(broken)
    assert data["valid"] and data["solarFlux"] == 168
    assert data["stale"] is True


def test_without_anything_in_hand_it_says_so():
    solar.reset_cache()

    def broken():
        raise OSError("giu'")

    data = solar.current(broken)
    assert data["valid"] is False
