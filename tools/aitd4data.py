"""Reading the data files of Alone in the Dark: The New Nightmare (PC, 2001).

Shared by the tools of this folder. What each format is: docs/FORMATOS.md.
"""

import os
import struct
import zlib

import numpy as np

# The game's folder: NEWNIGHTMARE_DATA, else a folder aitd4-data beside the
# project's (each tool also takes --data)
DEFAULT_DATA = os.environ.get("NEWNIGHTMARE_DATA") or os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "aitd4-data")


def read_file(path):
    """Contents of a data file, unpacked when it is gzip (most are; they are padded
    with zeros to 2048 bytes, the size of a CD sector, which gzip alone rejects)."""
    raw = open(path, "rb").read()
    if raw[:2] == b"\x1f\x8b":
        return zlib.decompressobj(31).decompress(raw)
    return raw


def levels(data):
    """Numbers of the levels in <data>\\levels, in order."""
    root = os.path.join(data, "levels")
    return sorted(int(n) for n in os.listdir(root) if n.isdigit())


def cameras(data, level):
    """Numbers of the cameras (view_NN) of a level, in order."""
    folder = os.path.join(data, "levels", str(level))
    found = set()
    for name in os.listdir(folder):
        low = name.lower()
        if low.startswith("view_") and low[5:7].isdigit():
            found.add(int(low[5:7]))
    return sorted(found)


def _morton_offsets(width, height):
    """Source offset of every pixel, as the game's FUN_004060c0: the pixels of
    an LMVR image are stored in Z (Morton) order, not row after row."""
    full = 0xFFFFFFFF
    if height < width:
        row_mask = (width * width - 1) & 0x55555555
        col_mask = (((width - height) * height) | 0xAAAAAAAA) & (width * height - 1)
        row_step = 0xAAAAAAAB
        col_step = (height * height - 1) & 0x55555556
    else:
        col_mask = (height * height - 1) & 0xAAAAAAAA
        row_mask = (((height - width) * width) | 0x55555555) & (width * height - 1)
        row_step = (width * width - 1) & 0xAAAAAAAB
        col_step = 0x55555556
    cols, v = [], 0
    for _ in range(width):
        cols.append(v)
        v = ((v + col_step) & full) & col_mask
    rows, v = [], 0
    for _ in range(height):
        rows.append(v)
        v = ((v + row_step) & full) & row_mask
    return np.array(rows, dtype=np.int64)[:, None] | np.array(cols, dtype=np.int64)[None, :]


LMVR_PIXELS = 0x3C
LMVR_TAIL = 2056  # two palettes of 256 BGRA colours and 8 more bytes


def read_lmvr(data):
    """Picture of an LMVR (or VRAM) file: (indices as a height x width array,
    palette as 256 x 3 RGB). Raises ValueError if it is not one."""
    if data[:4] not in (b"LMVR", b"VRAM"):
        raise ValueError("not an LMVR picture (starts with %r)" % data[:4])
    width, height = struct.unpack_from("<HH", data, 0x0C)
    if len(data) != LMVR_PIXELS + width * height + LMVR_TAIL:
        raise ValueError("unexpected size %d for %dx%d" % (len(data), width, height))
    pixels = np.frombuffer(data, np.uint8, width * height, LMVR_PIXELS)
    image = pixels[_morton_offsets(width, height)]
    tail = LMVR_PIXELS + width * height
    palette = np.frombuffer(data, np.uint8, 256 * 4, tail).reshape(256, 4)[:, 2::-1]  # stored as BGRA
    return image, palette


G3DB_CAMERA_FOV = 0x1C        # angle of view, radians
G3DB_POLYGON_COUNTS = 0x48    # polygons, indices, points
G3DB_POLYGON_TABLES = 0x94    # where they are (offsets in the block)
G3DB_POINT_SIZE = 0x24        # x, y, z, 3 floats (0 in the file), u, v, 1 float


def read_g3db_block(data):
    """The block of a G3DB chunk with its pointers turned into offsets of the
    block (FUN_0040410a): (slot, block bytes). Raises ValueError if it is not one."""
    if data[:4] != b"G3DB":
        raise ValueError("not a G3DB chunk (starts with %r)" % data[:4])
    slot, size, fixups = struct.unpack_from("<III", data, 4)
    if len(data) < 16 + size + 4 * fixups:  # other chunks may follow
        raise ValueError("unexpected size %d for a block of %d" % (len(data), size))
    return slot, data[16:16 + size]


def read_view_scene(data):
    """The scene of a camera (levels\\<level>\\view_NN): the camera and the 3D
    mesh of the room drawn in its background, as the flashlight lights it
    (FUN_004061b0). Points are in camera space: x right, y depth, z up; u, v
    is where the point is in the background sheet (0..1, of 1024 pixels).

    Returns a dict: position and angles of the camera in the world, to_world
    (world = to_world @ point + position), fov (radians), focal (pixels, for a
    640x480 picture), polygons as (group, [point numbers]) and points as
    (x, y, z, u, v).
    Each polygon is a strip of triangles (see strip_triangles). Group 0: the
    room; 1 and more: objects of the foreground."""
    _, block = read_g3db_block(data)
    scene = struct.unpack_from("<I", block, 0x10)[0]
    fov = struct.unpack_from("<f", block, scene + G3DB_CAMERA_FOV)[0]
    polygon_count, index_count, point_count = struct.unpack_from("<3I", block, scene + G3DB_POLYGON_COUNTS)
    polygon_table, index_table, point_table = struct.unpack_from("<3I", block, scene + G3DB_POLYGON_TABLES)
    indices = struct.unpack_from("<%dI" % index_count, block, index_table)
    polygons = []
    for k in range(polygon_count):
        group, first, count = struct.unpack_from("<3I", block, polygon_table + 12 * k)
        polygons.append((group, list(indices[first:first + count])))
    points = []
    for k in range(point_count):
        values = struct.unpack_from("<9f", block, point_table + G3DB_POINT_SIZE * k)
        points.append((values[0], values[1], values[2], values[6], values[7]))
    position = struct.unpack_from("<3f", block, scene + 0x04)
    angles = struct.unpack_from("<3f", block, scene + 0x10)
    return {
        "position": np.array(position),
        "angles": angles,
        "to_world": camera_rotation(angles),
        "fov": fov,
        "focal": 320 / np.tan(fov / 2),
        "polygons": polygons,
        "points": points,
    }


def camera_rotation(angles):
    """World from camera space for the angles of a camera: Rx(a0) Rz(a2)
    Ry(a1). Checked: with it the walls of the meshes fall on those of the
    level's collision map (the mirror one, a1 and a2 negated, does not)."""
    def turn(axis, a):
        c, s = np.cos(a), np.sin(a)
        if axis == 0:
            return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])
        if axis == 1:
            return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
        return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])
    return turn(0, angles[0]) @ turn(2, angles[2]) @ turn(1, angles[1])


def strip_triangles(numbers):
    """The triangles of a strip: each point with the two before it (every
    other one turned the other way round, so all face the same side)."""
    triangles = []
    for k in range(2, len(numbers)):
        a, b, c = numbers[k - 2:k + 1]
        triangles.append((a, b, c) if k % 2 == 0 else (a, c, b))
    return triangles


def project(scene, point):
    """Where a point of the scene falls in the 640x480 picture (FUN_004061b0)."""
    x, y, z = point[:3]
    focal = scene["focal"]
    return focal * x / y + 320, 240 - focal * z / y


def read_level_data(data):
    """The parts of the data of a level for a character (levels\\<level>\\
    CarnbyNN or AlineNN, FUN_0044bebd): the DRAM block (logic of the level,
    not studied) and the G3DB block (3D models and their textures, slot 0).
    Raises ValueError if it is not one."""
    if data[:4] != b"DRAM":
        raise ValueError("not level data (starts with %r)" % data[:4])
    size, pointers = struct.unpack_from("<II", data, 4)
    dram = data[12:12 + size]
    _, bank = read_g3db_block(data[12 + size + 4 * pointers:])
    return dram, bank


DRAM_MAGIC, DRAM_VERSION = 0xDEADBEEF, 0x0131554C
DRAM_STARTS = 0x3B4         # entries into the level, 0x24 bytes each
DRAM_COLLISION_MAP = 0x3E4  # the collision map
DRAM_TRIGGER_MAP = 0x3E8    # the trigger map
DRAM_PASS_TRIGGER_COUNT = 0x390
DRAM_PASS_TRIGGERS = 0x3AC  # what crossing each wall of the trigger map does


def read_level_logic(dram):
    """What is understood of the DRAM block of a level (FUN_0044a39d reads its
    header): a dict with start ((x, y, z), heading, camera: where the
    character comes in, or None), collision (read_collision_map, or None) and
    triggers (the walls of the trigger map: plane, edges, and from the pass
    trigger of the same number camera (only while it is shown, -1 any),
    enter and leave (places of scripts, 0 none)). Raises ValueError for other
    versions (the game refuses them)."""
    magic, version = struct.unpack_from("<II", dram, 0)
    if magic != DRAM_MAGIC or version != DRAM_VERSION:
        raise ValueError("DRAM of another version (%#x)" % version)
    starts, = struct.unpack_from("<I", dram, DRAM_STARTS)
    start = None
    if starts:
        values = struct.unpack_from("<6f", dram, starts)
        camera, = struct.unpack_from("<h", dram, starts + 0x1A)
        start = (values[:3], values[5], camera)
    collision_at, = struct.unpack_from("<I", dram, DRAM_COLLISION_MAP)
    triggers = []
    trigger_map, = struct.unpack_from("<I", dram, DRAM_TRIGGER_MAP)
    if trigger_map:
        walls, = struct.unpack_from("<h", dram, trigger_map + 4)
        walls_at, = struct.unpack_from("<I", dram, trigger_map + 0x10)
        count, = struct.unpack_from("<h", dram, DRAM_PASS_TRIGGER_COUNT)
        passes, = struct.unpack_from("<I", dram, DRAM_PASS_TRIGGERS)
        for k in range(min(walls, count)):
            wall = _read_trigger_wall(dram, walls_at + 0x68 * k)
            camera, enter, leave = struct.unpack_from("<hxxII", dram, passes + 0x10 * k + 4)
            wall.update({"camera": camera, "enter": enter, "leave": leave})
            triggers.append(wall)
    return {"start": start, "collision": read_collision_map(dram, collision_at) if collision_at else None,
            "triggers": triggers}


def script_camera(dram, script):
    """The camera a script sets (the first call of function 3 of the game,
    "push N; game 3"), or None."""
    if not script:
        return None
    pushed = None
    for _, name, values in disassemble_script(dram, script):
        if name == "push":
            pushed = values[0]
        elif name == "game" and values[0] == 3:
            return pushed
    return None


def _read_collision_plane(dram, at):
    kind, nx, ny, nz, d = struct.unpack_from("<i4f", dram, at)
    return {"kind": kind, "normal": np.array((nx, ny, nz)), "d": d}


def _read_collision_piece(dram, at, polygon):
    group, flags, height = struct.unpack_from("<hBB", dram, at)
    piece = {"group": group, "flags": flags, "height": height, "plane": _read_collision_plane(dram, at + 4), "edges": []}
    if polygon:
        count, = struct.unpack_from("<I", dram, at + 0x18)
        piece["edges"] = [_read_collision_plane(dram, at + 0x1C + 0x14 * e) for e in range(min(count, 4))]
    return piece


def _read_trigger_wall(dram, at):
    """A wall of the trigger map (0x68 bytes): a plane, number of edges and
    the edges, like a polygon of the collision map without its first 4 bytes."""
    count, = struct.unpack_from("<I", dram, at + 0x14)
    return {"plane": _read_collision_plane(dram, at),
            "edges": [_read_collision_plane(dram, at + 0x18 + 0x14 * e) for e in range(min(count, 4))]}


def read_collision_map(dram, at):
    """The collision map of a level (docs/FORMATOS.md): a dict with box (min
    and max, 3 floats each), cell_size, planes and polygons. A piece: group,
    flags (0x02 can be stood on), height, plane (kind, normal, d: the distance
    of a point is normal . point - d) and edges (planes; inside when the
    distance to all of them is 0 or more)."""
    box = struct.unpack_from("<6f", dram, at)
    cell_size = struct.unpack_from("<2f", dram, at + 0x18)
    plane_count, planes, polygon_count, polygons = struct.unpack_from("<4I", dram, at + 0x28)
    return {
        "box": (box[:3], box[3:]),
        "cell_size": cell_size,
        "planes": [_read_collision_piece(dram, planes + 0x18 * k, False) for k in range(plane_count)],
        "polygons": [_read_collision_piece(dram, polygons + 0x6C * k, True) for k in range(polygon_count)],
    }


# The instructions of the scripts (FUN_00411fc5): name and what follows the
# byte (h: u16, i: i32, hb: u16 and a byte)
SCRIPT_OPCODES = {
    0x00: ("nop", ""), 0x01: ("frame", "h"), 0x02: ("push bp", ""), 0x03: ("pop bp", ""),
    0x04: ("push", "i"), 0x05: ("push text", "h"), 0x06: ("push frame", "h"), 0x07: ("push frame text", "h"),
    0x08: ("push own", "h"), 0x09: ("push own text", "h"), 0x0A: ("pop own", "h"), 0x0B: ("pop frame", "h"),
    0x0C: ("pop own text", "h"), 0x0D: ("pop frame text", "h"), 0x0E: ("bp = sp", ""), 0x0F: ("drop", "h"),
    0x10: ("room", "h"), 0x11: ("call", "h"), 0x12: ("game", "h"), 0x13: ("return", ""), 0x14: ("end", ""),
    0x15: ("jump if", "h"), 0x16: ("jump unless", "h"), 0x17: ("jump", "h"), 0x18: ("==", ""), 0x19: ("!=", ""),
    0x1A: ("<", ""), 0x1B: (">", ""), 0x1C: ("<=", ""), 0x1D: (">=", ""), 0x1E: ("and", ""), 0x1F: ("or", ""),
    0x20: ("not", ""), 0x21: ("*", ""), 0x22: ("/", ""), 0x23: ("%", ""), 0x24: ("+", ""), 0x25: ("join texts", ""),
    0x26: ("-", ""), 0x27: ("negate", ""), 0x28: ("same truth", ""), 0x29: ("texts ==", ""), 0x2A: ("clear", "h"),
    0x2B: ("nop", ""), 0x3B: ("other truth", ""), 0x3C: ("texts !=", ""), 0x3D: ("pop game var", "h"),
    0x3E: ("pop game bit", "hb"), 0x3F: ("pop game text", "h"), 0x40: ("push game var", "h"),
    0x41: ("push game bit", "hb"), 0x42: ("push game text", "h"), 0x43: ("&", ""), 0x44: ("|", ""), 0x45: ("^", ""),
}


def disassemble_script(dram, at):
    """The instructions of the script at a place of a DRAM block: a list of
    (place in the code, name, operands). Raises ValueError if there is no
    script there. "game N" calls function N of the game (table at 0x4C7878;
    3 sets the camera)."""
    mark, own, length = struct.unpack_from("<IHH", dram, at)
    if mark != 2:
        raise ValueError("no script at %#x" % at)
    code = at + 0x0C
    pc, out = 0, []
    while pc < length:
        opcode = dram[code + pc]
        name, operands = SCRIPT_OPCODES.get(opcode, ("? %#x" % opcode, ""))
        values = []
        place = pc + 1
        for kind in operands:
            if kind == "h":
                values.append(struct.unpack_from("<H", dram, code + place)[0])
                place += 2
            elif kind == "i":
                values.append(struct.unpack_from("<i", dram, code + place)[0])
                place += 4
            else:
                values.append(dram[code + place])
                place += 1
        if opcode == 0x05:
            text_at = code + values[0]
            values.append(dram[text_at + 1:text_at + 1 + (dram[text_at] & 0x7F)].decode("latin-1"))
        out.append((pc, name, values))
        pc = place
        if opcode == 0x14 and pc >= length:
            break
    return out


def polygon_corners(piece):
    """The corners of a polygon of the collision map: where its plane meets
    each two edges one after the other."""
    plane, edges = piece["plane"], piece["edges"]
    corners = []
    for k in range(len(edges)):
        a, b = edges[k], edges[(k + 1) % len(edges)]
        matrix = np.array([plane["normal"], a["normal"], b["normal"]])
        if abs(np.linalg.det(matrix)) > 1e-6:
            corners.append(np.linalg.solve(matrix, np.array([plane["d"], a["d"], b["d"]])))
    return corners


def _twiddled_offsets(size):
    """Place of every pixel of a square texture of that size: the pixels are
    stored in Z order (bits of x and y interleaved, y first), as on the Dreamcast."""
    offsets = np.zeros((size, size), dtype=np.int64)
    coordinate = np.arange(size, dtype=np.int64)
    spread = np.zeros(size, dtype=np.int64)
    for bit in range(size.bit_length()):
        spread |= ((coordinate >> bit) & 1) << (2 * bit)
    offsets[:] = (spread[None, :] << 1) | spread[:, None]
    return offsets


def read_texture(bank, place):
    """A texture of a model (0x30 bytes at place in the bank): width x height
    x 4 array of RGBA. 16 bits a pixel in Z order; flag 0x01: RGB 5-6-5,
    otherwise ARGB 1-5-5-5 (FUN_00420c64). Flag 0x04: compressed as the
    Dreamcast's VQ (FUN_0048e037): a book of 256 blocks of 2x2 pixels, then a
    byte a block of the picture, both in Z order."""
    width, height, flags = struct.unpack_from("<HHI", bank, place)
    pixels_at = struct.unpack_from("<I", bank, place + 0x28)[0]
    if width != height:
        raise ValueError("texture of %dx%d: only square ones are known" % (width, height))
    if flags & 4:
        book = np.frombuffer(bank, np.uint16, 1024, pixels_at).reshape(256, 4).astype(np.uint32)
        half = width // 2
        blocks = np.frombuffer(bank, np.uint8, half * half, pixels_at + 2048)[_twiddled_offsets(half)].reshape(half, half)
        picture = np.zeros((width, width), np.uint32)
        for dy in range(2):
            for dx in range(2):
                picture[dy::2, dx::2] = book[blocks, dx * 2 + dy]
        p = picture
    else:
        raw = np.frombuffer(bank, np.uint16, width * height, pixels_at).astype(np.uint32)
        p = raw[_twiddled_offsets(width)]
    if flags & 1:
        r, g, b, a = (p >> 11) & 31, (p >> 5) & 63, p & 31, np.full_like(p, 1)
        g = g * 31 // 63
    else:
        r, g, b, a = (p >> 10) & 31, (p >> 5) & 31, p & 31, p >> 15
    rgba = np.stack([r * 255 // 31, g * 255 // 31, b * 255 // 31, a * 255], -1)
    return rgba.astype(np.uint8)


# Chunks read by readPointerBlock (FUN_0040410a): 4 letters, size, number of
# pointers, the block and the list of pointers. DRAM (FUN_00403ff3), HAND
# (FUN_00449395) and WEAP (FUN_0046d8bb)
POINTER_BLOCK_CHUNKS = (b"DRAM", b"HAND", b"WEAP")


def find_g3db_bank(data):
    """The block of the first G3DB chunk of a file, going over the chunks
    before it (lefthand\\carnby\\handN: HAND; weapon\\carnby\\weaponN: WEAP;
    levels\\<level>\\CarnbyNN: DRAM). Raises ValueError if there is none."""
    at = 0
    while at + 12 <= len(data):
        tag = data[at:at + 4]
        if tag == b"G3DB":
            return read_g3db_block(data[at:])[1]
        if tag not in POINTER_BLOCK_CHUNKS:
            raise ValueError("unknown chunk %r before the G3DB one" % tag)
        size, pointers = struct.unpack_from("<II", data, at + 4)
        at += 12 + size + 4 * pointers
    raise ValueError("no G3DB chunk")


G3DB_SKELETON_SIZE = 0x70  # one model with a skeleton in the bank


def read_skeleton_models(bank):
    """The models with a skeleton of a G3DB bank (characters, monsters,
    objects; FUN_004254d0, FUN_00425320, FUN_004896e0). Each one is a dict:
    bones: (offset from the parent (x, y, z), rest place (x, y, z), parent
        (-1: none), first vertex, end vertex): a bone moves its vertices
    vertices: (x, y, z, normal x, y, z, u, v) in the rest pose; z up, centimetres
    materials: (texture number or -1, flags: 0x01 both sides, 0x02 and 0x04
        two kinds of transparency)
    triangles: per material, a list of (vertex, vertex, vertex)
    textures: RGBA arrays (read_texture)
    The model also has a simpler copy for its shadow (not read yet)."""
    count, table = struct.unpack_from("<II", bank, 0)
    models = []
    for k in range(count):
        e = struct.unpack_from("<6I8I", bank, table + G3DB_SKELETON_SIZE * k)
        bone_count, strip_count, index_count, vertex_count, material_count, texture_count = e[:6]
        bones_at, _, _, strips_at, indices_at, vertices_at, materials_at, textures_at = e[6:14]
        bones = []
        for b in range(bone_count):
            v = struct.unpack_from("<6f3i", bank, bones_at + 0x24 * b)
            bones.append((v[0:3], v[3:6], v[6], v[7], v[8]))
        vertices = [struct.unpack_from("<8f", bank, vertices_at + 0x24 * n) for n in range(vertex_count)]
        indices = struct.unpack_from("<%dI" % index_count, bank, indices_at)
        materials = [struct.unpack_from("<iI", bank, materials_at + 8 * m) for m in range(material_count)]
        triangles = [[] for _ in range(material_count)]
        for s in range(strip_count):
            material, first, length = struct.unpack_from("<3I", bank, strips_at + 12 * s)
            # A strip: each index with the two before it, every other one turned
            a, b = indices[first], indices[first + 1]
            turn = False
            for j in range(2, length):
                c = indices[first + j]
                triangles[material].append((a, b, c))
                if turn:
                    b = c
                else:
                    a = c
                turn = not turn
        textures = [read_texture(bank, textures_at + 0x30 * t) for t in range(texture_count)]
        models.append({"bones": bones, "vertices": vertices, "materials": materials,
                       "triangles": triangles, "textures": textures})
    return models


def read_animations(bank, model_number):
    """The animations of a model with a skeleton of a bank (at +0x6C of the
    model; FUN_00427250). Returns (tracks, animations, table):
    tracks: per track (bone, position keys, angle keys, scale keys); a key is
        (x, y, z, time in milliseconds)
    animations: per animation a dict: length (ms), flags (0x01 loops, 0x02
        backwards, 0x04 moves at a steady speed), speed (cm a frame of 30 a
        second, with flag 0x04), rate (playing speed, 0x1000 = normal),
        tracks (track numbers)
    table: rows of 10 animation numbers (-1 none) from the bank, read up to
        the list of models (its meaning is not settled: docs/FORMATOS.md)
    A model without animations gives empty lists."""
    count, table_at = struct.unpack_from("<II", bank, 0)
    entry = table_at + G3DB_SKELETON_SIZE * model_number
    animation_set = struct.unpack_from("<I", bank, entry + 0x6C)[0]
    if animation_set == 0:
        return [], [], []
    track_count, animation_count, tracks_at, animations_at, numbers_at = struct.unpack_from("<HHIII", bank, animation_set)

    def keys(at, n):
        return [struct.unpack_from("<4f", bank, at + 16 * k) for k in range(n)]

    tracks = []
    for t in range(track_count):
        positions, angles, scales, bone, p_at, a_at, s_at = struct.unpack_from("<4H3I", bank, tracks_at + 0x14 * t)
        tracks.append((bone, keys(p_at, positions), keys(a_at, angles), keys(s_at, scales)))
    def track_list(at, n):
        return [x for x in struct.unpack_from("<%dH" % n, bank, at) if x < track_count] if n else []

    animations = []
    for a in range(animation_count):
        at = animations_at + 0x30 * a
        length, flags, n, n_clocked, n_mixes, n_fixed = struct.unpack_from("<6H", bank, at + 4)
        list_at, clocked_at, mixes_at, fixed_at = struct.unpack_from("<4I", bank, at + 0x18)
        speed = struct.unpack_from("<f", bank, at)[0]
        rate = struct.unpack_from("<H", bank, at + 0x14)[0]
        animation = {"length": length, "flags": flags, "speed": speed, "rate": rate,
                     "tracks": track_list(list_at, n), "clocked": [], "mixes": [], "fixed": []}
        for g in range(n_clocked):  # own clock: tracks, flags, length, list
            count, _, g_flags, g_length, g_list = struct.unpack_from("<4HI", bank, clocked_at + 0x10 * g)
            animation["clocked"].append({"length": g_length, "flags": g_flags, "tracks": track_list(g_list, count)})
        for g in range(n_mixes):  # sources mixed by weight slots
            count, _, g_flags, g_length, sources_at = struct.unpack_from("<4HI", bank, mixes_at + 0x10 * g)
            per_source = struct.unpack_from("<i", bank, sources_at)[0]
            sources = []
            for s in range(count):
                s_list, slot = struct.unpack_from("<Ii", bank, sources_at + 4 + 12 * s)
                sources.append({"tracks": track_list(s_list, per_source), "slot": slot})
            animation["mixes"].append({"length": g_length, "flags": g_flags, "sources": sources})
        for g in range(n_fixed):  # fixed poses, read at moment 0
            count = struct.unpack_from("<H", bank, fixed_at + 0x14 * g)[0]
            g_list = struct.unpack_from("<I", bank, fixed_at + 0x14 * g + 8)[0]
            animation["fixed"].append({"tracks": track_list(g_list, count)})
        animations.append(animation)
    table = []
    end = table_at if numbers_at < table_at else len(bank)
    for at in range(numbers_at, end - 19, 20):
        table.append(struct.unpack_from("<10h", bank, at))
    return tracks, animations, table


def _sample(keys, time, angles):
    """The value of a track's keys at a moment (FUN_00413130, FUN_004132e8)."""
    if not keys:
        return None
    for k, key in enumerate(keys):
        if angles and time == key[3]:
            return np.array(key[:3])
        if time < key[3]:
            if k == 0:
                return np.array(key[:3])
            before = keys[k - 1]
            change = np.array(key[:3]) - np.array(before[:3])
            if angles:
                change = (change + np.pi) % (2 * np.pi) - np.pi
            return np.array(before[:3]) + change * (time - before[3]) / (key[3] - before[3])
    return np.array(keys[-1][:3])


MIX_WEIGHTS = [0.25, 0.25, 0.25, 0.25] + [0.0] * 12  # as the game starts a model (FUN_004254d0)


def pose_skeleton(model, tracks, animation, time, weights=MIX_WEIGHTS):
    """4x4 matrices (for points as rows) of the bones of a model in the pose of
    an animation at a moment (FUN_00426ab0): every bone turns about z, then y,
    then x, is scaled, placed and carried by its parent. animation None: the
    rest pose. The animation's tracks, then its groups with their own clock,
    its mixes (sources summed by weight) and its fixed poses (FUN_00427250).
    The tracks of bone 0 (the root) are left out, as the game does
    (FUN_004279c0): they move the whole character, not the pose."""
    bones = model["bones"]
    place = [np.array(b[0], dtype=float) for b in bones]
    angles = [np.zeros(3) for _ in bones]
    scale = [np.ones(3) for _ in bones]

    def sample_tracks(track_numbers, at):
        for t in track_numbers:
            bone, positions, turns, scales = tracks[t]
            if bone == 0 or bone >= len(bones):
                continue
            for target, keys, is_angle in ((place, positions, False), (angles, turns, True), (scale, scales, False)):
                value = _sample(keys, at, is_angle)
                if value is not None:
                    target[bone] = value

    def own_time(length):
        return time % length if length else time

    if animation is not None:
        sample_tracks(animation["tracks"], time)
        for group in animation.get("clocked", []):
            sample_tracks(group["tracks"], own_time(group["length"]))
        for mix in animation.get("mixes", []):
            at = own_time(mix["length"])
            for k in range(len(mix["sources"][0]["tracks"]) if mix["sources"] else 0):
                bone = tracks[mix["sources"][0]["tracks"][k]][0]
                if bone == 0 or bone >= len(bones):
                    continue
                total_place, total_angles, total_scale = np.zeros(3), np.zeros(3), np.zeros(3)
                for source in mix["sources"]:
                    _, positions, turns, scales = tracks[source["tracks"][k]]
                    w = weights[source["slot"]]
                    value = _sample(positions, at, False)
                    total_place += (value if value is not None else np.array(bones[bone][0])) * w
                    value = _sample(turns, at, True)
                    if value is not None:
                        total_angles += value * w
                    value = _sample(scales, at, False)
                    total_scale += (value if value is not None else np.ones(3)) * w
                place[bone], angles[bone], scale[bone] = total_place, total_angles, total_scale
        for group in animation.get("fixed", []):
            sample_tracks(group["tracks"], 0.0)
    world = []
    for i, bone in enumerate(bones):
        m = np.eye(4)
        ax, ay, az = angles[i]
        c, s = np.cos(az), np.sin(az)
        m[:, [0, 1]] = np.stack([c * m[:, 0] - s * m[:, 1], s * m[:, 0] + c * m[:, 1]], 1)
        c, s = np.cos(ay), np.sin(ay)
        m[:, [0, 2]] = np.stack([c * m[:, 0] + s * m[:, 2], c * m[:, 2] - s * m[:, 0]], 1)
        c, s = np.cos(ax), np.sin(ax)
        m[:, [1, 2]] = np.stack([c * m[:, 1] - s * m[:, 2], s * m[:, 1] + c * m[:, 2]], 1)
        m[:3, :3] *= scale[i][None, :]
        m[3, :3] += place[i]
        if 0 <= bone[2] < i:
            m = m @ world[bone[2]]
        world.append(m)
    return world


def skin_vertices(model, world):
    """The vertices (x, y, z, normal, u, v) of a model with its bones in a pose:
    every vertex goes with its bone from the bone's rest place."""
    vertices = np.array(model["vertices"], dtype=float)
    out = vertices.copy()
    for i, bone in enumerate(model["bones"]):
        first, end = bone[3], bone[4]
        if end <= first:
            continue
        places = np.c_[vertices[first:end, :3] - np.array(bone[1]), np.ones(end - first)] @ world[i]
        out[first:end, :3] = places[:, :3]
        out[first:end, 3:6] = vertices[first:end, 3:6] @ world[i][:3, :3]
    return out



VRM_HEADER = 96


def read_vrm(data):
    """A picture of doc\\ or book\\ (.VRM, made with the Dreamcast tool
    DcBitmap): 96 bytes of header ("picture", "USAGE: DcBitmap", at 0x40 the
    sheet's width and height, at 0x46 the picture's width and height) and the
    sheet, 16 bits a pixel row after row, colour 5-5-5. Returns the visible
    picture as height x width x 3 RGB. Raises ValueError if it is not one."""
    if len(data) < VRM_HEADER or b"DcBitmap" not in data[:VRM_HEADER]:
        raise ValueError("not a DcBitmap picture")
    width, height = struct.unpack_from("<HH", data, 0x40)
    shown_width, shown_height = struct.unpack_from("<HH", data, 0x46)
    if len(data) < VRM_HEADER + width * height * 2 or shown_width > width or shown_height > height:
        raise ValueError("unexpected size %d for %dx%d" % (len(data), width, height))
    p = np.frombuffer(data, np.uint16, width * height, VRM_HEADER).reshape(height, width).astype(np.uint32)
    p = p[:shown_height, :shown_width]
    rgb = np.stack([(p >> 10) & 31, (p >> 5) & 31, p & 31], -1) * 255 // 31
    return rgb.astype(np.uint8)
