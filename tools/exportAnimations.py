"""Makes a catalogue of the animations of a model: a sheet with every
animation drawn halfway through (seen three-quarters on, so steps show) and a
CSV list with its length, whether it loops, what kind of movement it is (from
how far apart the feet get) and where it appears in the bank's table of
animation numbers. To put names to the animations by looking at them.

The model is, by default, the biggest one of the file (the character).

Examples:
  python tools/exportAnimations.py                         Carnby, level 1
  python tools/exportAnimations.py --file Aline01
  python tools/exportAnimations.py --level 1 --file Carnby01 --model 8

Output: <data>\\exports\\animations\\<level>_<file>_model_NN.png and .csv
(the data copy, never the GOG install).
"""

import argparse
import csv
import os

import numpy as np
from PIL import Image, ImageDraw

import aitd4data

TILE = 120       # pixels of each drawing
COLUMNS = 16
TURN = np.radians(-55)  # three-quarters view


def draw(model, vertices, size=TILE):
    """A small drawing of a model with its textures (software, slow but
    enough for a catalogue). vertices: x, y, z, normal, u, v."""
    c, s = np.cos(TURN), np.sin(TURN)
    x = vertices[:, 0] * c - vertices[:, 1] * s
    depth = vertices[:, 0] * s + vertices[:, 1] * c
    z = vertices[:, 2]
    scale = (size - 12) / 200.0  # 2 metres fill the drawing
    sx = size / 2 + (x - (x.min() + x.max()) / 2) * scale  # centred: some animations move away
    sy = size - 6 - z * scale
    image = np.full((size, size, 3), 30, np.uint8)
    zbuffer = np.full((size, size), -1e9)
    for material, triangles in zip(model["materials"], model["triangles"]):
        texture = model["textures"][material[0]] if material[0] >= 0 else None
        for a, b, cc in triangles:
            px = np.array([sx[a], sx[b], sx[cc]])
            py = np.array([sy[a], sy[b], sy[cc]])
            area = (py[1] - py[2]) * (px[0] - px[2]) + (px[2] - px[1]) * (py[0] - py[2])
            if abs(area) < 1e-6:
                continue
            x0, x1 = max(int(px.min()), 0), min(int(px.max()) + 1, size - 1)
            y0, y1 = max(int(py.min()), 0), min(int(py.max()) + 1, size - 1)
            if x0 > x1 or y0 > y1:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            w0 = ((py[1] - py[2]) * (gx - px[2]) + (px[2] - px[1]) * (gy - py[2])) / area
            w1 = ((py[2] - py[0]) * (gx - px[2]) + (px[0] - px[2]) * (gy - py[2])) / area
            w2 = 1 - w0 - w1
            inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if not inside.any():
                continue
            d = w0 * depth[a] + w1 * depth[b] + w2 * depth[cc]
            region = zbuffer[y0:y1 + 1, x0:x1 + 1]
            visible = inside & (d > region)
            if not visible.any():
                continue
            region[visible] = d[visible]
            if texture is None:
                colour = np.array([150, 150, 150])
                image[y0:y1 + 1, x0:x1 + 1][visible] = colour
                continue
            u = w0 * vertices[a, 6] + w1 * vertices[b, 6] + w2 * vertices[cc, 6]
            v = w0 * vertices[a, 7] + w1 * vertices[b, 7] + w2 * vertices[cc, 7]
            n = texture.shape[0]
            tu = np.clip((u * n).astype(int), 0, n - 1)
            tv = np.clip((v * n).astype(int), 0, n - 1)
            image[y0:y1 + 1, x0:x1 + 1][visible] = texture[tv[visible], tu[visible], :3]
    return image


def feet_bones(model):
    """The two lowest bones with vertices (the feet)."""
    with_vertices = [i for i, b in enumerate(model["bones"]) if b[4] > b[3]]
    return sorted(with_vertices, key=lambda i: model["bones"][i][1][2])[:2]


def movement(model, tracks, animation, feet):
    """still, steps, walk or run, from how far apart the feet get front to back."""
    spread = []
    for time in np.linspace(0, animation["length"], 7):
        v = aitd4data.skin_vertices(model, aitd4data.pose_skeleton(model, tracks, animation, time))
        a, b = (v[model["bones"][f][3]:model["bones"][f][4], 1].mean() for f in feet)
        spread.append(a - b)
    span = max(spread) - min(spread)
    return "run" if span > 150 else "walk" if span > 100 else "steps" if span > 40 else "still"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", default=aitd4data.DEFAULT_DATA, help="folder of the game's data")
    parser.add_argument("--level", type=int, default=1)
    parser.add_argument("--file", default="Carnby01")
    parser.add_argument("--model", type=int, help="model of the file (default the biggest)")
    args = parser.parse_args()

    _, bank = aitd4data.read_level_data(aitd4data.read_file(os.path.join(args.data, "levels", str(args.level), args.file)))
    models = aitd4data.read_skeleton_models(bank)
    number = args.model if args.model is not None else max(range(len(models)), key=lambda k: len(models[k]["vertices"]))
    model = models[number]
    tracks, animations, table = aitd4data.read_animations(bank, number)
    places = {}
    for r, row in enumerate(table):
        for c, a in enumerate(row):
            if a >= 0:
                places.setdefault(a, []).append("%d.%d" % (r, c))
    feet = feet_bones(model)

    out_folder = os.path.join(args.data, "exports", "animations")
    os.makedirs(out_folder, exist_ok=True)
    name = "%d_%s_model_%02d" % (args.level, args.file, number)
    rows = (len(animations) + COLUMNS - 1) // COLUMNS
    sheet = Image.new("RGB", (COLUMNS * TILE, max(rows, 1) * TILE))
    with open(os.path.join(out_folder, name + ".csv"), "w", newline="") as listing:
        writer = csv.writer(listing)
        writer.writerow(["animation", "length_ms", "loops", "backwards", "movement", "table_row.column"])
        for k, animation in enumerate(animations):
            world = aitd4data.pose_skeleton(model, tracks, animation, animation["length"] * 0.5)
            tile = Image.fromarray(draw(model, aitd4data.skin_vertices(model, world)))
            kind = movement(model, tracks, animation, feet)
            ImageDraw.Draw(tile).text((3, 2), "%d %s" % (k, kind), fill=(255, 255, 0))
            sheet.paste(tile, ((k % COLUMNS) * TILE, (k // COLUMNS) * TILE))
            writer.writerow([k, animation["length"], animation["flags"] & 1, (animation["flags"] >> 1) & 1, kind,
                             " ".join(places.get(k, []))])
    sheet.save(os.path.join(out_folder, name + ".png"))
    print("%d animations in %s" % (len(animations), os.path.join(out_folder, name + ".png")))


if __name__ == "__main__":
    main()
