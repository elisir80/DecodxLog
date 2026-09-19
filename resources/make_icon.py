# DecoLog — l'icona.
#
# L'icona e' disegnata a mano (resources/icon/decolog-icon.svg): la nuvola del
# Cloud con le righe del log dentro, l'onda del segnale sotto e il nome. Le
# taglie sono gia' esportate dall'SVG in resources/icon/decolog-<n>.png, e la
# piccola (decolog-icon-small.svg) e' un disegno a parte, senza il nome, per
# quando lo spazio non basta a leggerlo.
#
# Questo script non disegna niente: mette insieme le taglie esportate nei due
# file che servono alla compilazione —
#
#   resources/decolog.ico   l'eseguibile su Windows (dal .rc): Esplora risorse,
#                           la barra delle applicazioni, le proprieta' del file
#   resources/decolog.png   la finestra e i dialoghi (risorsa Qt, 256 pixel)
#
# Si rilancia solo quando l'icona cambia:
#
#   python resources/make_icon.py
#
# Niente Pillow: un .ico e' un indice piu' i PNG attaccati in fondo, e copiarli
# dentro e' meglio che ridisegnarli — le taglie piccole sono ritoccate a mano e
# una riduzione automatica le rovinerebbe.

import os
import shutil
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "icon")

# Le taglie che Windows cerca: 16 e 32 nelle liste, 48 nelle cartelle, 256 per
# le icone grandi e l'anteprima. Oltre i 256 non serve: il formato .ico le
# vuole scritte come 0 e i programmi vecchi si confondono.
SIZES = [16, 24, 32, 48, 64, 128, 256]


def png_at(size):
    path = os.path.join(SOURCE, "decolog-%d.png" % size)
    with open(path, "rb") as f:
        data = f.read()
    width, height = struct.unpack(">II", data[16:24])
    if (width, height) != (size, size):
        raise SystemExit("%s non e' %dx%d ma %dx%d" % (path, size, size, width, height))
    return data


def write_ico(path, images):
    """L'indice: sei byte di intestazione, sedici per taglia, poi i PNG interi."""
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = len(header) + 16 * len(images)
    index = b""
    for size, data in images:
        # 256 si scrive 0: nel formato il campo e' di un byte solo.
        index += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32,
                             len(data), offset)
        offset += len(data)
    with open(path, "wb") as f:
        f.write(header + index + b"".join(data for _, data in images))


def main():
    images = [(size, png_at(size)) for size in SIZES]
    write_ico(os.path.join(HERE, "decolog.ico"), images)
    shutil.copyfile(os.path.join(SOURCE, "decolog-256.png"),
                    os.path.join(HERE, "decolog.png"))
    print("decolog.ico  %s" % " ".join(str(s) for s in SIZES))
    print("decolog.png  256")


if __name__ == "__main__":
    main()
