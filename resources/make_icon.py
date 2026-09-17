# DecoLog — generatore dell'icona.
#
# Il simbolo e' il log: le righe dei QSO, con la barretta di stato a sinistra che
# nell'applicazione dice "nuovo DXCC" (verde accento) o "gia' lavorato" (blu), e
# l'arco del segnale che entra da destra — i QSO che arrivano dalla radio. Palette
# identica al resto della famiglia Decodium (tema Ocean Blue), come in DecoRTTY.
#
# Le taglie grandi sono disegnate in alta risoluzione e ridotte; 16 e 32 pixel
# hanno un disegno proprio, con due righe sole, perche' a quelle dimensioni tre
# righe e un arco diventano una macchia.

from PIL import Image, ImageDraw

BG_DEEP   = (10, 15, 26)
BG_MEDIUM = (17, 24, 39)
PRIMARY   = (74, 144, 226)
SECONDARY = (0, 212, 255)
ACCENT    = (0, 255, 136)
TEXT_DIM  = (137, 180, 208)

# Le righe del log: (lunghezza relativa, colore della barretta di stato).
# La prima e' il QSO appena arrivato, ed e' quella verde.
ROWS = [(0.96, ACCENT), (0.78, PRIMARY), (0.88, SECONDARY)]


def mix(color, fraction, bg=(14, 20, 33)):
    """Colore composto a mano sullo sfondo del pannello: l'alpha di Pillow su una
    immagine RGBA schiarisce invece di fondere, e le righe venivano bianche."""
    return tuple(int(bg[i] + (color[i] - bg[i]) * fraction) for i in range(3)) + (255,)


def rounded_panel(size, radius_frac=0.22):
    """Il pannello di vetro su cui poggia tutto, con il bordo della famiglia."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = int(size * radius_frac)

    for y in range(size):
        t = y / max(1, size - 1)
        c = tuple(int(BG_DEEP[i] + (BG_MEDIUM[i] - BG_DEEP[i]) * t) for i in range(3))
        d.line([(0, y), (size, y)], fill=c + (255,))

    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius=r, fill=255)
    img.putalpha(mask)

    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, size - 1, size - 1], radius=r,
                        outline=mix(PRIMARY, 0.55), width=max(1, size // 48))
    return img


def draw_log(img, simple=False):
    """Le righe del log con la barretta di stato, e l'arco del segnale."""
    size = img.size[0]
    d = ImageDraw.Draw(img, "RGBA")

    rows = ROWS[:2] if simple else ROWS
    # A 16 pixel le righe prendono tutta la larghezza: l'arco non ci sta.
    x0 = size * 0.17
    x1 = size * (0.92 if simple else 0.72)
    top = size * (0.30 if simple else 0.26)
    gap = size * (0.22 if simple else 0.155)
    height = size * (0.13 if simple else 0.105)
    tick = max(2, int(size * (0.075 if simple else 0.055)))

    for i, (length, color) in enumerate(rows):
        y = top + i * gap
        # La barretta di stato: e' lei che da' il colore alla riga.
        d.rounded_rectangle([x0, y, x0 + tick, y + height],
                            radius=tick / 2, fill=color + (255,))
        text_x0 = x0 + tick * 2
        text_x1 = text_x0 + (x1 - text_x0) * length
        # Il testo della riga: una barra piena, il log non si legge a 32 pixel.
        # La prima riga e' viva come il QSO appena arrivato, le altre sono log.
        bar = mix(ACCENT, 0.82) if i == 0 else mix(TEXT_DIM, 0.42)
        d.rounded_rectangle([text_x0, y, text_x1, y + height], radius=height / 2, fill=bar)

    if simple:
        return img

    # L'arco del segnale che entra da destra: due archi concentrici e il punto
    # della stazione, come le onde sulla mappa.
    cx, cy = size * 0.855, size * 0.775
    d.ellipse([cx - size * 0.028, cy - size * 0.028, cx + size * 0.028, cy + size * 0.028],
              fill=ACCENT + (255,))
    for k, fraction in ((0.09, 0.95), (0.15, 0.6), (0.21, 0.33)):
        r = size * k
        d.arc([cx - r, cy - r, cx + r, cy + r], start=185, end=355,
              fill=mix(ACCENT, fraction), width=max(1, int(size * 0.022)))
    return img


def build(size, simple=False):
    return draw_log(rounded_panel(size), simple=simple)


def main():
    small = {16: build(16, simple=True), 32: build(32, simple=True)}
    big = build(1024)
    frames = []
    for s in (16, 24, 32, 48, 64, 128, 256):
        if s in small:
            frames.append(small[s])
        elif s == 24:
            frames.append(build(96, simple=True).resize((24, 24), Image.LANCZOS))
        else:
            frames.append(big.resize((s, s), Image.LANCZOS))

    frames[-1].save("decolog.ico", format="ICO",
                    sizes=[(f.size[0], f.size[1]) for f in frames],
                    append_images=frames[:-1])
    big.resize((256, 256), Image.LANCZOS).save("decolog.png")
    # Anteprima: le taglie una accanto all'altra, per guardarle come le vedra'
    # l'operatore nella barra delle applicazioni.
    preview = Image.new("RGBA", (16 + 24 + 32 + 48 + 64 + 128 + 256 + 8 * 7, 264), (0, 0, 0, 0))
    x = 4
    for f in frames:
        preview.paste(f, (x, 260 - f.size[1]), f)
        x += f.size[0] + 8
    preview.save("icon_preview.png")
    print("scritti decolog.ico (%s), decolog.png e icon_preview.png"
          % ", ".join(str(f.size[0]) for f in frames))


if __name__ == "__main__":
    main()
