"""Exports the backgrounds of the cameras as PNG pictures.

Each camera has two: S (lit, what the flashlight shows) and D (in the dark).
They are 640x480; with --full the whole 1024-wide sheet is saved, with the
extra pieces the game keeps below the picture.

Examples:
  python tools/exportViews.py                     every level
  python tools/exportViews.py --level 1           one level
  python tools/exportViews.py --level 1 --camera 3
  python tools/exportViews.py --file <path>       one file (view_NND / view_NNS)

Output: <data>\\exports\\views\\<level>\\view_NN_S.png and view_NN_D.png
(the data copy, never the GOG install).
"""

import argparse
import os
import sys

from PIL import Image

import aitd4data

SCENE_WIDTH, SCENE_HEIGHT = 640, 480


def to_png(path, out, full):
    image, palette = aitd4data.read_lmvr(aitd4data.read_file(path))
    if not full:
        image = image[:SCENE_HEIGHT, :SCENE_WIDTH]
    os.makedirs(os.path.dirname(out), exist_ok=True)
    Image.fromarray(palette[image]).save(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", default=aitd4data.DEFAULT_DATA, help="folder of the game's data")
    parser.add_argument("--out", help="output folder (default <data>\\exports\\views)")
    parser.add_argument("--level", type=int)
    parser.add_argument("--camera", type=int)
    parser.add_argument("--file", help="export only this file")
    parser.add_argument("--full", action="store_true", help="whole 1024-wide sheet")
    args = parser.parse_args()
    out_root = args.out or os.path.join(args.data, "exports", "views")

    if args.file:
        name = os.path.basename(args.file)
        to_png(args.file, os.path.join(out_root, name + ".png"), args.full)
        print("ok", name)
        return

    level_list = [args.level] if args.level is not None else aitd4data.levels(args.data)
    done = failed = 0
    for level in level_list:
        folder = os.path.join(args.data, "levels", str(level))
        camera_list = [args.camera] if args.camera is not None else aitd4data.cameras(args.data, level)
        for camera in camera_list:
            for kind in ("S", "D"):
                source = os.path.join(folder, "view_%02d%s" % (camera, kind))
                if not os.path.exists(source):
                    continue
                target = os.path.join(out_root, str(level), "view_%02d_%s.png" % (camera, kind))
                try:
                    to_png(source, target, args.full)
                    done += 1
                except ValueError as error:
                    failed += 1
                    print("skipped %s: %s" % (source, error), file=sys.stderr)
        print("level %d done" % level)
    print("%d pictures, %d skipped, in %s" % (done, failed, out_root))


if __name__ == "__main__":
    main()
