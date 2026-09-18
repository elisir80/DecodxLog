"""DecoLog Cloud — il tema della stazione, anche nel browser.

I colori non sono una scelta della pagina: arrivano con le impostazioni
sincronizzate. Queste prove guardano che siano quelli del ThemeManager, valore
per valore, e che una stazione senza impostazioni non resti al buio.
"""

from __future__ import annotations

from decolog_cloud import theme


def test_without_settings_it_is_the_default_theme():
    t = theme.theme_of({})
    assert t["name"] == "Ocean Blue"
    assert t["vars"]["bg-deep"] == "#0A0F1A"
    assert t["vars"]["primary"] == "#4A90E2"
    assert "--bg-deep: #0A0F1A;" in t["css"]


def test_the_theme_comes_from_the_station_settings():
    t = theme.theme_of({"theme/current": "Darkcodium"})
    assert t["name"] == "Darkcodium"
    assert t["vars"]["bg-deep"] == "#050706"
    assert t["light"] is False

    light = theme.theme_of({"theme/current": "Stellar Light"})
    assert light["light"] is True
    assert light["vars"]["text"] == "#0E1A22"


def test_the_accent_variant_only_bites_in_darkcodium():
    amber = theme.theme_of({"theme/current": "Darkcodium", "theme/accentVariant": "amber"})
    assert amber["vars"]["accent"] == "#ffb820"
    assert amber["variant"] == "amber"

    # In Ocean Blue la variante non c'e': l'accento resta quello del tema.
    ocean = theme.theme_of({"theme/current": "Ocean Blue", "theme/accentVariant": "amber"})
    assert ocean["vars"]["accent"] == "#00FF88"
    assert ocean["variant"] == ""


def test_density_becomes_the_row_height():
    assert theme.theme_of({"theme/density": "compact"})["vars"]["row-height"] == "20px"
    assert theme.theme_of({"theme/density": "comfortable"})["vars"]["font-size"] == "14px"
    # Una densita' che non esiste non rompe niente.
    assert theme.theme_of({"theme/density": "boh"})["vars"]["row-height"] == "24px"


def test_custom_colours_win_when_they_are_on():
    t = theme.theme_of({"theme/current": "Ocean Blue", "theme/customEnabled": "true",
                        "theme/customBg": "#101010", "theme/customText": "#eeeeee"})
    assert t["vars"]["bg-deep"] == "#101010"
    assert t["vars"]["text"] == "#eeeeee"
    # Spenti, non contano.
    off = theme.theme_of({"theme/current": "Ocean Blue", "theme/customEnabled": "false",
                          "theme/customBg": "#101010"})
    assert off["vars"]["bg-deep"] == "#0A0F1A"


def test_a_packed_setting_does_not_become_a_colour():
    # I valori impacchettati di Qt arrivano come dizionari: si ignorano.
    t = theme.theme_of({"theme/current": {"__qvariant__": "AAAA"}})
    assert t["name"] == "Ocean Blue"
