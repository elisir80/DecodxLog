"""DecoDXLog Cloud — gli stessi colori del programma, anche nel browser.

I temi non sono una scelta della pagina: sono quelli della stazione, e stanno
gia' sul Cloud fra le impostazioni sincronizzate (`theme/current`,
`theme/accentVariant`, `theme/density`, i colori personalizzati). Qui quei
valori diventano variabili CSS, con gli stessi numeri di
`libs/decodium-ui/src/ThemeManager.cpp`: chi apre il log dal browser lo vede
come lo vede sul suo computer.
"""

from __future__ import annotations

# I tre temi, valore per valore come nel ThemeManager.
PALETTES = {
    "Ocean Blue": {
        "bg-deep": "#0A0F1A", "bg-panel": "#111827", "bg-medium": "#1E2D42",
        "primary": "#4A90E2", "secondary": "#00D4FF", "accent": "#00FF88",
        "warning": "#FF8C00", "error": "#FF5F56", "success": "#4CAF50",
        "text": "#E8F4FD", "text-dim": "#89B4D0",
        "border": "rgba(74, 144, 226, 0.31)", "border-soft": "rgba(74, 144, 226, 0.16)",
        "panel": "#1E2D42", "panel-head": "#283C57",
        "row-match": "rgba(0, 255, 136, 0.15)",
        "light": False,
    },
    "Stellar Light": {
        "bg-deep": "#EDF2F7", "bg-panel": "#E1E9F1", "bg-medium": "#FFFFFF",
        "primary": "#1F76D2", "secondary": "#0E9AAE", "accent": "#0E8C6A",
        "warning": "#B5741A", "error": "#CE4038", "success": "#0E8C6A",
        "text": "#0E1A22", "text-dim": "#5C6E7E",
        "border": "#C3D2DF", "border-soft": "#DFE8F0",
        "panel": "#FFFFFF", "panel-head": "#EAF1F7",
        "row-match": "rgba(14, 140, 106, 0.14)",
        "light": True,
    },
    "Darkcodium": {
        "bg-deep": "#050706", "bg-panel": "#0d1310", "bg-medium": "#182019",
        "primary": "#19ff88", "secondary": "#66e6ff", "accent": "#19ff88",
        "warning": "#ffb84a", "error": "#ff5466", "success": "#19ff88",
        "text": "#d6dcd8", "text-dim": "#6c7872",
        "border": "rgba(31, 42, 34, 0.8)", "border-soft": "rgba(31, 42, 34, 0.5)",
        "panel": "#0d1310", "panel-head": "#0a0e0c",
        "row-match": "rgba(25, 255, 136, 0.11)",
        "light": False,
    },
}

# Le varianti d'accento di Darkcodium: accento, spento, profondo.
ACCENTS = {
    "phosphor": ("#19ff88", "#0fa55a", "#052d1a"),
    "cyan": ("#66e6ff", "#1b9fcc", "#04222d"),
    "amber": ("#ffb820", "#a06d10", "#2e1d04"),
    "red": ("#ff5466", "#a82c3a", "#2e090f"),
}

# Le densita': altezza riga, testo, testata di pannello.
DENSITIES = {
    "compact": (20, 12, 26),
    "regular": (24, 13, 30),
    "comfortable": (28, 14, 34),
}

DEFAULT = "Ocean Blue"


def theme_of(settings: dict | None) -> dict:
    """Le variabili CSS della stazione, dalle impostazioni arrivate dal programma."""
    settings = settings or {}

    def value(key: str, fallback: str) -> str:
        raw = settings.get(key)
        if isinstance(raw, dict) or raw in (None, ""):
            return fallback
        return str(raw)

    name = value("theme/current", DEFAULT)
    palette = dict(PALETTES.get(name, PALETTES[DEFAULT]))
    variant = value("theme/accentVariant", "phosphor")
    density = value("theme/density", "regular")

    # L'accento si sceglie solo in Darkcodium, come nel programma.
    if name == "Darkcodium":
        accent, dim, deep = ACCENTS.get(variant, ACCENTS["phosphor"])
        palette["accent"] = accent
        palette["primary"] = accent
        palette["accent-dim"] = dim
        palette["accent-deep"] = deep
        palette["row-match"] = _rgba(accent, 0.11)
    palette.setdefault("accent-dim", palette["primary"])
    palette.setdefault("accent-deep", palette["bg-medium"])

    row, font, head = DENSITIES.get(density, DENSITIES["regular"])
    palette["row-height"] = f"{row}px"
    palette["font-size"] = f"{font}px"
    palette["panel-height"] = f"{head}px"

    # I colori personalizzati vincono, come in ThemeManager::customBg/customText.
    if str(settings.get("theme/customEnabled", "")).lower() in ("true", "1"):
        custom_bg = value("theme/customBg", "")
        custom_text = value("theme/customText", "")
        if custom_bg:
            palette["bg-deep"] = custom_bg
            palette["bg-panel"] = _elevate(custom_bg, 1.18)
            palette["bg-medium"] = _elevate(custom_bg, 1.35)
            palette["panel"] = palette["bg-panel"]
            palette["panel-head"] = _elevate(custom_bg, 1.08)
        if custom_text:
            palette["text"] = custom_text

    return {
        "name": name,
        "variant": variant if name == "Darkcodium" else "",
        "density": density,
        "vars": palette,
        "css": "".join(f"--{k}: {v};" for k, v in palette.items() if k != "light"),
        "light": bool(palette.get("light")),
    }


def _rgba(hex_color: str, alpha: float) -> str:
    r, g, b = _rgb(hex_color)
    return f"rgba({r}, {g}, {b}, {alpha})"


def _rgb(hex_color: str) -> tuple[int, int, int]:
    value = hex_color.lstrip("#")
    if len(value) != 6:
        return (0, 0, 0)
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))   # type: ignore[return-value]


def _elevate(hex_color: str, factor: float) -> str:
    """Lo stesso schiarimento di ThemeManager::elevate, per i colori scelti a mano."""
    r, g, b = _rgb(hex_color)
    out = [min(255, int(c * factor) + 6) for c in (r, g, b)]
    return "#%02x%02x%02x" % tuple(out)
