"""Exports the pictures of doc\\ and book\\ (.VRM: documents, book pages and
close-ups the game shows full screen) as PNG pictures. The files of other
kinds in those folders (smaller ones, DRAM) are skipped and listed.

Examples:
  python tools/exportDocuments.py
  python tools/exportDocuments.py --folder book

Output: <data>\\exports\\documents\\<folder>\\<name>.png (the data copy, never
the GOG install).
"""

import argparse
import os
import sys

from PIL import Image

import aitd4data


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", default=aitd4data.DEFAULT_DATA, help="folder of the game's data")
    parser.add_argument("--folder", choices=("doc", "book"), help="only this folder")
    args = parser.parse_args()
    done = skipped = 0
    for folder in [args.folder] if args.folder else ["doc", "book"]:
        out_folder = os.path.join(args.data, "exports", "documents", folder)
        os.makedirs(out_folder, exist_ok=True)
        for name in sorted(os.listdir(os.path.join(args.data, folder))):
            path = os.path.join(args.data, folder, name)
            if os.path.isdir(path):
                continue
            try:
                picture = aitd4data.read_vrm(aitd4data.read_file(path))
            except ValueError as error:
                skipped += 1
                print("skipped %s: %s" % (path, error), file=sys.stderr)
                continue
            Image.fromarray(picture).save(os.path.join(out_folder, os.path.splitext(name)[0] + ".png"))
            done += 1
    print("%d pictures, %d files skipped, in %s" % (done, skipped, os.path.join(args.data, "exports", "documents")))


if __name__ == "__main__":
    main()
