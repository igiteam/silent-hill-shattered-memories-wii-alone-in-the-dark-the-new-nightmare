"""Draws the 3D mesh of each camera (G3DB, levels\\<level>\\view_NN) over its
dark background, to check that the scene is read right: the lines must follow
the edges of the room. Blue: the room (group 0), green: group 1, yellow:
group 2 and more (groups 1 and more are the objects of the foreground).

Needs the backgrounds exported first (tools/exportViews.py).

Examples:
  python tools/drawViewMeshes.py --level 1 --camera 1
  python tools/drawViewMeshes.py --level 1

Output: <data>\\exports\\meshes\\<level>\\view_NN.png (the data copy, never
the GOG install).
"""

import argparse
import os
import sys

from PIL import Image, ImageDraw

import aitd4data

COLOURS = {0: (80, 140, 255), 1: (80, 230, 120)}
FOREGROUND = (255, 220, 60)


def draw(scene_path, background_path, out):
    scene = aitd4data.read_view_scene(aitd4data.read_file(scene_path))
    picture = Image.open(background_path).convert("RGB")
    pen = ImageDraw.Draw(picture)
    for group, numbers in scene["polygons"]:
        for triangle in aitd4data.strip_triangles(numbers):
            corners = [aitd4data.project(scene, scene["points"][n]) for n in triangle]
            pen.polygon(corners, outline=COLOURS.get(group, FOREGROUND))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    picture.save(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", default=aitd4data.DEFAULT_DATA, help="folder of the game's data")
    parser.add_argument("--level", type=int)
    parser.add_argument("--camera", type=int)
    args = parser.parse_args()
    views = os.path.join(args.data, "exports", "views")
    out_root = os.path.join(args.data, "exports", "meshes")

    level_list = [args.level] if args.level is not None else aitd4data.levels(args.data)
    done = failed = 0
    for level in level_list:
        camera_list = [args.camera] if args.camera is not None else aitd4data.cameras(args.data, level)
        for camera in camera_list:
            name = "view_%02d" % camera
            scene_path = os.path.join(args.data, "levels", str(level), name)
            background = os.path.join(views, str(level), name + "_D.png")
            try:
                draw(scene_path, background, os.path.join(out_root, str(level), name + ".png"))
                done += 1
            except (OSError, ValueError) as error:
                failed += 1
                print("skipped %s: %s" % (scene_path, error), file=sys.stderr)
    print("%d meshes drawn, %d skipped, in %s" % (done, failed, out_root))


if __name__ == "__main__":
    main()
