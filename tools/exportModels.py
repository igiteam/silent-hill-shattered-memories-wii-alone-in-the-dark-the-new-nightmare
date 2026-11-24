"""Exports the 3D models with a skeleton of the levels (characters, monsters,
objects) as OBJ files with their materials and textures in PNG, to look at
them in any 3D viewer (the 3D Viewer of Windows, Blender...). They are in
their rest pose; the skeleton and the animations are not exported.

They come from the data of each level for each character: levels\\<level>\\
CarnbyNN and AlineNN (docs/FORMATOS.md). Carnby is model 1 of Carnby01 and
Aline model 0 of Aline01 in most levels.

With --parts it exports instead the hands and weapons the characters hold
(lefthand\\<character>\\handN, weapon\\<character>\\...), to
<data>\\exports\\models\\parts\\<character>\\<file>\\model_NN.obj.

Examples:
  python tools/exportModels.py --parts
  python tools/exportModels.py --level 1
  python tools/exportModels.py --level 1 --file Carnby01 --model 1
  python tools/exportModels.py                     every level

Output: <data>\\exports\\models\\<level>\\<file>\\model_NN.obj (.mtl and
model_NN_tT.png next to it; the data copy, never the GOG install).
"""

import argparse
import os
import struct
import sys

from PIL import Image

import aitd4data


def write_obj(model, folder, name):
    """Writes one model as name.obj, name.mtl and its textures. OBJ files have
    y up: the game's (x, y, z) with z up becomes (x, z, -y)."""
    os.makedirs(folder, exist_ok=True)
    texture_names = []
    for t, rgba in enumerate(model["textures"]):
        texture_names.append("%s_t%d.png" % (name, t))
        Image.fromarray(rgba).save(os.path.join(folder, texture_names[-1]))
    with open(os.path.join(folder, name + ".mtl"), "w") as mtl:
        for m, (texture, flags) in enumerate(model["materials"]):
            mtl.write("newmtl material%d\nKd 1 1 1\n" % m)
            if 0 <= texture < len(texture_names):
                mtl.write("map_Kd %s\n" % texture_names[texture])
            mtl.write("\n")
    with open(os.path.join(folder, name + ".obj"), "w") as obj:
        obj.write("# Alone in the Dark: The New Nightmare model, rest pose (centimetres)\n")
        obj.write("mtllib %s.mtl\n" % name)
        for x, y, z, nx, ny, nz, u, v in model["vertices"]:
            obj.write("v %.4f %.4f %.4f\n" % (x, z, -y))
            obj.write("vn %.4f %.4f %.4f\n" % (nx, nz, -ny))
            obj.write("vt %.5f %.5f\n" % (u, 1.0 - v))
        for m, triangles in enumerate(model["triangles"]):
            if not triangles:
                continue
            obj.write("usemtl material%d\n" % m)
            for a, b, c in triangles:
                obj.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % ((a + 1,) * 3 + (b + 1,) * 3 + (c + 1,) * 3))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", default=aitd4data.DEFAULT_DATA, help="folder of the game's data")
    parser.add_argument("--level", type=int)
    parser.add_argument("--file", help="only this file of the level (Carnby01, Aline01...)")
    parser.add_argument("--model", type=int, help="only this model of the file")
    parser.add_argument("--parts", action="store_true", help="the hands and weapons instead of the levels")
    args = parser.parse_args()
    out_root = os.path.join(args.data, "exports", "models")

    if args.parts:
        done = 0
        for folder in ("lefthand", "weapon"):
            for character in sorted(os.listdir(os.path.join(args.data, folder))):
                for name in sorted(os.listdir(os.path.join(args.data, folder, character))):
                    path = os.path.join(args.data, folder, character, name)
                    try:
                        models = aitd4data.read_skeleton_models(aitd4data.find_g3db_bank(aitd4data.read_file(path)))
                    except (ValueError, IndexError, struct.error) as error:
                        print("skipped %s: %s" % (path, error), file=sys.stderr)
                        continue
                    for k, model in enumerate(models):
                        write_obj(model, os.path.join(out_root, "parts", character, name), "model_%02d" % k)
                        done += 1
        print("%d models in %s" % (done, os.path.join(out_root, "parts")))
        return

    level_list = [args.level] if args.level is not None else aitd4data.levels(args.data)
    done = failed = 0
    for level in level_list:
        folder = os.path.join(args.data, "levels", str(level))
        for name in sorted(os.listdir(folder)):
            if not name.startswith(("Carnby", "Aline")) or (args.file and name.lower() != args.file.lower()):
                continue
            try:
                _, bank = aitd4data.read_level_data(aitd4data.read_file(os.path.join(folder, name)))
                models = aitd4data.read_skeleton_models(bank)
            except (ValueError, IndexError, struct.error) as error:  # a few files are not like the rest
                failed += 1
                print("skipped %s: %s" % (os.path.join(folder, name), error), file=sys.stderr)
                continue
            for k, model in enumerate(models):
                if args.model is not None and k != args.model:
                    continue
                write_obj(model, os.path.join(out_root, str(level), name), "model_%02d" % k)
                done += 1
    print("%d models, %d files skipped, in %s" % (done, failed, out_root))


if __name__ == "__main__":
    main()
