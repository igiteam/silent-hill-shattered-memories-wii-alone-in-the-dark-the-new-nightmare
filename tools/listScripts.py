"""Lists the scripts of a level that are understood so far, instruction by
instruction: the level's script (run when entering it), the entry's script,
and the scripts of the pass triggers (crossing the walls of the trigger map),
of the zones (going in, out, the action button inside) and the numbered ones,
with the functions of the game they call. Also says which functions of the
game the level's scripts call, and how often.

Examples:
  python tools/listScripts.py --level 1
  python tools/listScripts.py --level 144 --file Aline01

Output: on the screen.
"""

import argparse
import os
import struct
from collections import Counter

import aitd4data

DRAM_PASS_TRIGGER_COUNT = 0x390
DRAM_PASS_TRIGGERS = 0x3AC
DRAM_LEVEL_SCRIPT = 0x14


def show(dram, name, at, calls):
    if not at:
        return
    print("== %s (%#x)" % (name, at))
    try:
        for pc, opcode, values in aitd4data.disassemble_script(dram, at):
            print("  %04x  %-16s %s" % (pc, opcode, " ".join(str(v) for v in values)))
            if opcode == "game":
                calls[values[0]] += 1
    except (ValueError, struct.error, IndexError) as error:
        print("  (cannot be read: %s)" % error)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data", default=aitd4data.DEFAULT_DATA, help="folder of the game's data")
    parser.add_argument("--level", type=int, default=1)
    parser.add_argument("--file", default="Carnby01", help="data of the level for a character")
    args = parser.parse_args()

    path = os.path.join(args.data, "levels", str(args.level), args.file)
    dram, _ = aitd4data.read_level_data(aitd4data.read_file(path))
    aitd4data.read_level_logic(dram)  # checks the version
    calls = Counter()
    show(dram, "level script", struct.unpack_from("<I", dram, DRAM_LEVEL_SCRIPT)[0], calls)
    starts = struct.unpack_from("<I", dram, aitd4data.DRAM_STARTS)[0]
    if starts:
        camera, = struct.unpack_from("<h", dram, starts + 0x1A)
        script, = struct.unpack_from("<I", dram, starts + 0x20)
        print("== entry: camera %d" % camera)
        show(dram, "entry script", script, calls)
    count, = struct.unpack_from("<h", dram, DRAM_PASS_TRIGGER_COUNT)
    triggers, = struct.unpack_from("<I", dram, DRAM_PASS_TRIGGERS)
    for k in range(max(count, 0)):
        camera, enter, leave = struct.unpack_from("<hxxII", dram, triggers + 0x10 * k + 4)
        when = "always" if camera < 0 else "in camera %d" % camera
        show(dram, "pass trigger %d, entering, %s" % (k, when), enter, calls)
        show(dram, "pass trigger %d, leaving, %s" % (k, when), leave, calls)
    count, = struct.unpack_from("<h", dram, 0x392)
    zones, = struct.unpack_from("<I", dram, 0x3B0)
    for k in range(max(count, 0) if zones else 0):
        enter, leave, action = struct.unpack_from("<III", dram, zones + 0x20 * k + 4)
        show(dram, "zone %d, entering" % k, enter, calls)
        show(dram, "zone %d, leaving" % k, leave, calls)
        show(dram, "zone %d, action" % k, action, calls)
    count, = struct.unpack_from("<h", dram, 0x3AA)
    events, = struct.unpack_from("<I", dram, 0x3E0)
    for k in range(max(count, 0) if events else 0):
        number, script = struct.unpack_from("<iI", dram, events + 8 * k)
        show(dram, "numbered script %d" % number, script, calls)
    print("== functions of the game called: " + ", ".join("%d (%d times)" % item for item in sorted(calls.items())))


if __name__ == "__main__":
    main()
