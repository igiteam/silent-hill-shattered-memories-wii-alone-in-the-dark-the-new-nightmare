"""Draws the collision map of a level seen from above: the floors that can be
stood on in green, the walls in red, the walls of the meshes of its cameras
(their points over 20 cm, taken to the world) in blue, the cameras as black
dots with their number and where the character comes in as an orange ring.
The walls of the trigger map are purple lines with a short mark towards
their front and "a>b": crossing it from its front, while camera a is shown
(* any), its script sets camera b (? if it does something else). Useful to
check a level, and that cameras and maps agree.

Examples:
  python tools/drawCollisionMap.py --level 1
  python tools/drawCollisionMap.py --level 2 --file Aline01

Output: <data>\\exports\\collision\\level_<N>_<file>.png (the data copy, never
the GOG install).
"""

import argparse
import os

import numpy as np
from PIL import Image, ImageDraw

import aitd4data

SCALE = 0.4  # pixels a centimetre
MARGIN = 20


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", default=aitd4data.DEFAULT_DATA, help="folder of the game's data")
    parser.add_argument("--level", type=int, default=1)
    parser.add_argument("--file", default="Carnby01", help="data of the level for a character")
    args = parser.parse_args()

    folder = os.path.join(args.data, "levels", str(args.level))
    dram, _ = aitd4data.read_level_data(aitd4data.read_file(os.path.join(folder, args.file)))
    logic = aitd4data.read_level_logic(dram)
    collision = logic["collision"]
    if collision is None:
        raise SystemExit("level %d has no collision map in %s" % (args.level, args.file))
    (x0, y0, _), (x1, y1, _) = collision["box"]
    width, height = int((x1 - x0) * SCALE) + 2 * MARGIN, int((y1 - y0) * SCALE) + 2 * MARGIN
    picture = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(picture)

    def place(p):
        return MARGIN + (p[0] - x0) * SCALE, height - MARGIN - (p[1] - y0) * SCALE

    for piece in collision["polygons"]:
        corners = aitd4data.polygon_corners(piece)
        if len(corners) >= 2:
            floor = piece["plane"]["normal"][2] > 0.7
            draw.polygon([place(p) for p in corners], outline=(0, 160, 0) if floor else (220, 0, 0))

    for camera in aitd4data.cameras(args.data, args.level):
        scene = aitd4data.read_view_scene(aitd4data.read_file(os.path.join(folder, "view_%02d" % camera)))
        points = np.array([p[:3] for p in scene["points"]]) @ scene["to_world"].T + scene["position"]
        for p in points[points[:, 2] > 20]:
            draw.point(place(p), fill=(0, 0, 255))
        x, y = place(scene["position"])
        draw.ellipse((x - 4, y - 4, x + 4, y + 4), fill="black")
        draw.text((x + 5, y), str(camera), fill="black")

    for wall in logic["triggers"]:
        corners = aitd4data.polygon_corners(wall)
        if len(corners) < 2:
            continue
        points = [place(p) for p in corners]
        draw.polygon(points, outline=(150, 0, 200))
        x = sum(p[0] for p in points) / len(points)
        y = sum(p[1] for p in points) / len(points)
        normal = wall["plane"]["normal"]
        draw.line([(x, y), (x + normal[0] * 12, y - normal[1] * 12)], fill=(150, 0, 200), width=2)
        target = aitd4data.script_camera(dram, wall["enter"])
        draw.text((x + 4, y + 2), "%s>%s" % ("*" if wall["camera"] < 0 else wall["camera"],
                                              "?" if target is None else target), fill=(110, 0, 160))

    if logic["start"]:
        x, y = place(logic["start"][0])
        draw.ellipse((x - 7, y - 7, x + 7, y + 7), outline=(255, 140, 0), width=3)

    out_folder = os.path.join(args.data, "exports", "collision")
    os.makedirs(out_folder, exist_ok=True)
    out = os.path.join(out_folder, "level_%d_%s.png" % (args.level, args.file))
    picture.save(out)
    print("%s: %d planes, %d polygons, %d trigger walls" % (out, len(collision["planes"]), len(collision["polygons"]),
                                                            len(logic["triggers"])))


if __name__ == "__main__":
    main()
