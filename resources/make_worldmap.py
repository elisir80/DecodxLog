# DecoDXLog — la mappa grande dell'orologio mondiale.
#
# Scarica da Natural Earth (pubblico dominio) le terre emerse e i confini di
# stato alla scala 1:50m e li riduce a quello che serve a una mappa di un
# migliaio di pixel: Douglas-Peucker a 0,03° e coordinate a due decimali.
# La mappa piccola (quella del log e della barra in basso) resta la 110m.
#
#   python resources/make_worldmap.py

import json
import os
import urllib.request

BASE = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/"
LAND = BASE + "ne_50m_land.geojson"
BORDERS = BASE + "ne_50m_admin_0_boundary_lines_land.geojson"
HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "map")
TOLERANCE = 0.03   # gradi


def perpendicular_distance(p, a, b):
    if a == b:
        return ((p[0] - a[0]) ** 2 + (p[1] - a[1]) ** 2) ** 0.5
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / (dx * dx + dy * dy)
    t = max(0.0, min(1.0, t))
    px, py = a[0] + t * dx, a[1] + t * dy
    return ((p[0] - px) ** 2 + (p[1] - py) ** 2) ** 0.5


def simplify(points, tolerance):
    """Douglas-Peucker iterativo: i poligoni grandi farebbero saltare la ricorsione."""
    if len(points) < 3:
        return points
    keep = [False] * len(points)
    keep[0] = keep[-1] = True
    stack = [(0, len(points) - 1)]
    while stack:
        first, last = stack.pop()
        worst, index = 0.0, 0
        for i in range(first + 1, last):
            d = perpendicular_distance(points[i], points[first], points[last])
            if d > worst:
                worst, index = d, i
        if worst > tolerance:
            keep[index] = True
            stack.append((first, index))
            stack.append((index, last))
    return [p for p, k in zip(points, keep) if k]


def flat(points):
    out = []
    for lon, lat in points:
        out += [round(lon, 2), round(lat, 2)]
    return out


def fetch(url):
    with urllib.request.urlopen(url, timeout=120) as r:
        return json.loads(r.read().decode("utf-8"))


def rings_of(geometry):
    if geometry["type"] == "Polygon":
        return geometry["coordinates"]
    if geometry["type"] == "MultiPolygon":
        return [ring for poly in geometry["coordinates"] for ring in poly]
    return []


def lines_of(geometry):
    if geometry["type"] == "LineString":
        return [geometry["coordinates"]]
    if geometry["type"] == "MultiLineString":
        return geometry["coordinates"]
    return []


def main():
    land = fetch(LAND)
    rings = []
    for feature in land["features"]:
        for ring in rings_of(feature["geometry"]):
            small = simplify(ring, TOLERANCE)
            if len(small) >= 4:
                rings.append(flat(small))
    borders = fetch(BORDERS)
    lines = []
    for feature in borders["features"]:
        for line in lines_of(feature["geometry"]):
            small = simplify(line, TOLERANCE)
            if len(small) >= 2:
                lines.append(flat(small))
    with open(os.path.join(HERE, "land50.json"), "w", encoding="utf-8") as f:
        json.dump({"source": "Natural Earth 50m land (public domain)", "rings": rings}, f, separators=(",", ":"))
    with open(os.path.join(HERE, "borders50.json"), "w", encoding="utf-8") as f:
        json.dump({"source": "Natural Earth 50m admin 0 boundary lines (public domain)", "lines": lines},
                  f, separators=(",", ":"))
    print(len(rings), "rings,", len(lines), "border lines")


if __name__ == "__main__":
    main()
