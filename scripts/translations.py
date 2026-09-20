# -*- coding: utf-8 -*-
"""Le traduzioni di DecoDXLog: quante ne mancano, e da dove si prendono.
#
#   python scripts/translations.py stato            quanto e' tradotta ogni lingua
#   python scripts/translations.py mancanti de 40   le prime 40 che mancano in tedesco
#   python scripts/translations.py da-decodium      riprende quelle gia' fatte in Decodium 4
#   python scripts/translations.py applica de file  scrive le traduzioni di un file JSON
#
# Le quindici lingue sono quelle di Decodium 4: chi passa da un programma
# all'altro trova la sua, e con le stesse parole.
"""
import io
import json
import os
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TRANSLATIONS = os.path.join(HERE, "translations")
DECODIUM = r"C:\Decodium-4.0\translations"

# codice → (nome nella lingua stessa, nome in italiano)
LANGUAGES = [
    ("it", "Italiano", "italiano"),
    ("en", "English", "inglese"),
    ("de", "Deutsch", "tedesco"),
    ("fr", "Français", "francese"),
    ("es", "Español", "spagnolo"),
    ("ca", "Català", "catalano"),
    ("nl", "Nederlands", "olandese"),
    ("da", "Dansk", "danese"),
    ("hu", "Magyar", "ungherese"),
    ("ro", "Română", "rumeno"),
    ("lv", "Latviešu", "lettone"),
    ("ru", "Русский", "russo"),
    ("ja", "日本語", "giapponese"),
    ("zh", "简体中文", "cinese semplificato"),
    ("zh_TW", "繁體中文", "cinese tradizionale"),
]


def path_of(code):
    return os.path.join(TRANSLATIONS, "decodxlog_%s.ts" % code)


def read(path):
    """Le voci di un .ts: source → (translation, tradotta?)."""
    out = {}
    if not os.path.exists(path):
        return out
    tree = ET.parse(path)
    for msg in tree.iter("message"):
        src = msg.find("source")
        tr = msg.find("translation")
        if src is None or src.text is None:
            continue
        if msg.get("numerus") == "yes":
            forms = [f.text or "" for f in msg.iter("numerusform")]
            done = bool(forms) and all(forms) and tr is not None and tr.get("type") != "unfinished"
            out[src.text] = ("\n".join(forms), done)
            continue
        text = (tr.text or "") if tr is not None else ""
        done = bool(text) and (tr is None or tr.get("type") != "unfinished")
        out[src.text] = (text, done)
    return out


def stato():
    base = read(path_of("it"))
    total = len(base)
    print("%-6s %-22s %8s %8s  %s" % ("cod", "lingua", "fatte", "mancano", "quanto"))
    for code, native, italian in LANGUAGES:
        if code == "en":
            # L'inglese e' la lingua in cui sono scritte le stringhe: non ha file.
            print("%-6s %-22s %8s %8s  %s" % (code, native, total, 0, "sorgente"))
            continue
        entries = read(path_of(code))
        if not entries:
            print("%-6s %-22s %8s %8s  %s" % (code, native, 0, total, "— il file non c'e' ancora"))
            continue
        done = sum(1 for _, ok in entries.values() if ok)
        missing = len(entries) - done
        bar = "#" * int(round(20.0 * done / max(1, len(entries))))
        print("%-6s %-22s %8d %8d  %-20s %3.0f%%"
              % (code, native, done, missing, bar, 100.0 * done / max(1, len(entries))))


def mancanti(code, limit):
    entries = read(path_of(code))
    missing = [src for src, (_, ok) in entries.items() if not ok]
    for src in missing[:limit]:
        print(json.dumps(src, ensure_ascii=False))
    print("# mancano %d in totale" % len(missing), file=sys.stderr)


def da_decodium():
    """Quello che Decodium 4 ha gia' tradotto, con lo stesso testo inglese."""
    base = read(path_of("it"))
    for code, native, _ in LANGUAGES:
        if code in ("en", "it"):
            continue
        theirs_path = os.path.join(DECODIUM, "decodium_%s.ts" % code)
        mine_path = path_of(code)
        if not os.path.exists(theirs_path) or not os.path.exists(mine_path):
            continue
        theirs = {src: text for src, (text, ok) in read(theirs_path).items() if ok}
        taken = apply_map(mine_path, {src: theirs[src] for src in base if src in theirs})
        print("%-6s %s: riprese %d voci da Decodium" % (code, native, taken))


def apply_map(path, mapping):
    """Scrive le traduzioni nel .ts, lasciando stare quelle gia' fatte."""
    if not mapping:
        return 0
    raw = io.open(path, encoding="utf-8", newline="").read()
    crlf = "\r\n" in raw
    text = raw.replace("\r\n", "\n")
    written = 0
    for src, translation in mapping.items():
        if not translation:
            continue
        escaped = (src.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
                      .replace('"', "&quot;").replace("'", "&apos;"))
        value = (translation.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
                            .replace('"', "&quot;").replace("'", "&apos;"))
        for form in (escaped, src):
            needle = "<source>%s</source>\n        <translation type=\"unfinished\"></translation>" % form
            if needle in text:
                text = text.replace(needle,
                                    "<source>%s</source>\n        <translation>%s</translation>" % (form, value), 1)
                written += 1
                break
    io.open(path, "w", encoding="utf-8", newline="\r\n" if crlf else "\n").write(text)
    return written


def applica(code, json_path):
    with io.open(json_path, encoding="utf-8") as handle:
        mapping = json.load(handle)
    written = apply_map(path_of(code), mapping)
    print("%s: scritte %d traduzioni" % (code, written))


if __name__ == "__main__":
    command = sys.argv[1] if len(sys.argv) > 1 else "stato"
    if command == "stato":
        stato()
    elif command == "mancanti":
        mancanti(sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 50)
    elif command == "da-decodium":
        da_decodium()
    elif command == "applica":
        applica(sys.argv[2], sys.argv[3])
    else:
        print(__doc__)
