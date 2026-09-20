# DecoDXLog — le coste della mappa.
#
# Scarica il "ne_110m_coastline" di Natural Earth (pubblico dominio) e lo riduce a
# quello che serve a una mappa piccola: linee spezzate all'antimeridiano,
# semplificate con Douglas-Peucker e arrotondate a un decimo di grado. Da 140 kB
# di GeoJSON restano circa 29 kB.
#
#   python resources/make_coastline.py

import io
import json
import os
import urllib.request

URL = ("https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/"
       "geojson/ne_110m_coastline.geojson")
DST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "map", "coastline.json")
TOLERANCE = 0.35   # gradi


def perpendicular_distance(p, a, b):
    if a == b:
        return ((p[0] - a[0]) ** 2 + (p[1] - a[1]) ** 2) ** 0.5
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / (dx * dx + dy * dy)
    t = max(0.0, min(1.0, t))
    px, py = a[0] + t * dx, a[1] + t * dy
    return ((p[0] - px) ** 2 + (p[1] - py) ** 2) ** 0.5


def simplify(points, tolerance):
    """Douglas-Peucker: tiene i punti che cambiano davvero la forma."""
    if len(points) < 3:
        return points
    worst, index = 0.0, 0
    for i in range(1, len(points) - 1):
        d = perpendicular_distance(points[i], points[0], points[-1])
        if d > worst:
            worst, index = d, i
    if worst <= tolerance:
        return [points[0], points[-1]]
    return simplify(points[:index + 1], tolerance)[:-1] + simplify(points[index:], tolerance)


def lines_from(geometry):
    kind = geometry["type"]
    if kind == "LineString":
        return [geometry["coordinates"]]
    if kind == "MultiLineString":
        return list(geometry["coordinates"])
    return []


def main():
    print("scarico", URL)
    with urllib.request.urlopen(URL, timeout=60) as response:
        data = json.loads(response.read().decode("utf-8"))

    out = []
    before = after = 0
    for feature in data["features"]:
        for line in lines_from(feature["geometry"]):
            pts = [(round(lon, 3), round(lat, 3)) for lon, lat in line]
            before += len(pts)
            # Una linea che salta l'antimeridiano, disegnata cosi' com'e',
            # attraverserebbe la mappa da parte a parte.
            chunks, current = [], [pts[0]]
            for previous, point in zip(pts, pts[1:]):
                if abs(point[0] - previous[0]) > 180:
                    chunks.append(current)
                    current = []
                current.append(point)
            chunks.append(current)
            for chunk in chunks:
                if len(chunk) < 2:
                    continue
                simple = simplify(chunk, TOLERANCE)
                if len(simple) < 2:
                    continue
                after += len(simple)
                out.append([[round(lon, 1), round(lat, 1)] for lon, lat in simple])

    text = '{"source":"Natural Earth 110m coastline (public domain)","lines":[\n'
    text += ",\n".join(json.dumps(line, separators=(",", ":")) for line in out)
    text += "\n]}\n"
    os.makedirs(os.path.dirname(DST), exist_ok=True)
    io.open(DST, "w", encoding="utf-8").write(text)
    print("linee %d · punti %d -> %d · %d byte" % (len(out), before, after, len(text)))


if __name__ == "__main__":
    main()
