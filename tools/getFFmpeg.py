"""Downloads FFmpeg (LGPL, shared, 64-bit Windows; builds of BtbN on GitHub)
into ThirdParty/ffmpeg: only what the engine uses to play the game's Bink
videos (headers, import libraries and DLLs of avcodec, avformat, avutil,
swresample and swscale) and its licence. The binaries are not kept in Git.

Example:
  python tools/getFFmpeg.py
"""

import io
import os
import shutil
import sys
import urllib.request
import zipfile

URL = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n8.1-latest-win64-lgpl-shared-8.1.zip"
LIBRARIES = ("avcodec", "avformat", "avutil", "swresample", "swscale")


def main():
    target = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ThirdParty", "ffmpeg")
    source = sys.argv[1] if len(sys.argv) > 1 else None
    if source:
        data = open(source, "rb").read()
    else:
        print("Downloading", URL)
        data = urllib.request.urlopen(URL).read()
    archive = zipfile.ZipFile(io.BytesIO(data))
    if os.path.isdir(target):
        shutil.rmtree(target)
    kept = 0
    for name in archive.namelist():
        parts = name.split("/", 1)
        if len(parts) < 2 or not parts[1] or name.endswith("/"):
            continue
        inner = parts[1]
        folder = inner.split("/")[0]
        base = os.path.basename(inner)
        wanted = (inner == "LICENSE.txt"
                  or (folder == "include" and inner.split("/")[1] in ["lib" + l for l in LIBRARIES])
                  or (folder == "lib" and base in [l + ".lib" for l in LIBRARIES])
                  or (folder == "bin" and base.endswith(".dll") and base.split("-")[0] in LIBRARIES))
        if not wanted:
            continue
        out = os.path.join(target, *inner.split("/"))
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "wb") as f:
            f.write(archive.read(name))
        kept += 1
    print(kept, "files in", os.path.normpath(target))


if __name__ == "__main__":
    main()
