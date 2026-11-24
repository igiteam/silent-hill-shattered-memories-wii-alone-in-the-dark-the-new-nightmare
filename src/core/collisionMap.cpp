#include "collisionMap.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr uint16_t kEmpty = 0x8000, kPlaneBit = 0x8000, kEnd = 0xFFFF;
constexpr size_t kPlaneSize = 0x18, kPolygonSize = 0x6C, kEdgeSize = 0x14;
constexpr uint32_t kMaxEdges = 4;

// After bumping, the sphere stays this far in front of the piece (FUN_004029ee)
constexpr float kStandOff = 1.5f;

// Reads values of a block, noting when a read falls outside it
struct BlockReader {
    const uint8_t* data;
    size_t size;
    bool ok = true;

    template <typename T> T get(size_t at)
    {
        T value{};
        if (at + sizeof(T) > size) {
            ok = false;
            return value;
        }
        std::memcpy(&value, data + at, sizeof(T));
        return value;
    }
};

// A plane of 0x14 bytes: kind, normal, d
CollisionPlane readPlane(BlockReader& r, size_t at)
{
    CollisionPlane plane;
    plane.kind = r.get<int32_t>(at);
    for (int i = 0; i < 3; i++)
        plane.normal[i] = r.get<float>(at + 4 + 4 * size_t(i));
    plane.d = r.get<float>(at + 0x10);
    return plane;
}

// A piece: group, flags, height, its plane and, for a polygon, its edges
CollisionPiece readPiece(BlockReader& r, size_t at, bool polygon)
{
    CollisionPiece piece;
    piece.group = r.get<int16_t>(at);
    piece.flags = r.get<uint8_t>(at + 2);
    piece.height = r.get<uint8_t>(at + 3);
    piece.plane = readPlane(r, at + 4);
    if (polygon) {
        const uint32_t edges = std::min(r.get<uint32_t>(at + 0x18), kMaxEdges);
        for (uint32_t e = 0; e < edges; e++)
            piece.edges.push_back(readPlane(r, at + 0x1C + kEdgeSize * e));
    }
    return piece;
}

bool insideEdges(const CollisionPiece& piece, const float p[3])
{
    for (const CollisionPlane& edge : piece.edges)
        if (planeDistance(edge, p) < 0)
            return false;
    return true;
}

// The cell of a point along one axis, kept inside the grid
int cellOf(const CollisionMap& map, int axis, float value)
{
    const int cell = int(std::floor((value - map.boxMin[axis]) / map.cellSize[axis]));
    return std::clamp(cell, 0, kCollisionCells - 1);
}

// Whether a piece counts: its group is on (a group the map has not: on)
bool pieceOn(const CollisionMap& map, const CollisionPiece& piece)
{
    return piece.group < 0 || size_t(piece.group) >= map.groupOn.size() || map.groupOn[size_t(piece.group)] != 0;
}

// Calls visit for every piece of a cell that counts
template <typename Visit> void forEachPiece(const CollisionMap& map, int cell, Visit visit)
{
    const uint16_t start = map.cells[size_t(cell)];
    if (start & kEmpty)
        return;
    for (size_t k = start; k < map.lists.size() && map.lists[k] != kEnd; k++) {
        const uint16_t number = map.lists[k];
        const CollisionPiece& piece = (number & kPlaneBit) ? map.planes[number & 0x7FFF] : map.polygons[number];
        if (pieceOn(map, piece))
            visit(piece);
    }
}

// The first piece the sphere meets going from one point to the other
// (FUN_00401d4a, FUN_00405e13): it meets a piece when it starts in front of
// it (farther than its radius) and ends behind, and the point where it
// crosses is inside the piece's edges
struct Hit {
    float fraction = 2.0f;
    float centre[3] = {};
    float normal[3] = {};
};

bool sweep(const CollisionMap& map, const float from[3], const float to[3], float radius, Hit& hit)
{
    hit = Hit{};
    const int x0 = cellOf(map, 0, std::min(from[0], to[0]) - radius);
    const int x1 = cellOf(map, 0, std::max(from[0], to[0]) + radius);
    const int y0 = cellOf(map, 1, std::min(from[1], to[1]) - radius);
    const int y1 = cellOf(map, 1, std::max(from[1], to[1]) + radius);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            forEachPiece(map, y * kCollisionCells + x, [&](const CollisionPiece& piece) {
                const float before = planeDistance(piece.plane, from) - radius;
                const float after = planeDistance(piece.plane, to) - radius;
                if (before < 0 || after >= 0)
                    return;
                const float fraction = before / (before - after);
                float centre[3];
                for (int i = 0; i < 3; i++)
                    centre[i] = from[i] + (to[i] - from[i]) * fraction;
                if (fraction >= hit.fraction || !insideEdges(piece, centre))
                    return;
                hit.fraction = fraction;
                std::memcpy(hit.centre, centre, sizeof(centre));
                std::memcpy(hit.normal, piece.plane.normal, sizeof(hit.normal));
            });
    return hit.fraction <= 1.0f;
}

} // namespace

float planeDistance(const CollisionPlane& plane, const float point[3])
{
    return plane.normal[0] * point[0] + plane.normal[1] * point[1] + plane.normal[2] * point[2] - plane.d;
}

bool readCollisionPlane(const uint8_t* block, size_t size, size_t at, CollisionPlane& out)
{
    BlockReader r{ block, size };
    out = readPlane(r, at);
    return r.ok;
}

bool readCollisionMap(const uint8_t* block, size_t size, uint32_t at, CollisionMap& out)
{
    out = CollisionMap{};
    BlockReader r{ block, size };
    for (int i = 0; i < 3; i++) {
        out.boxMin[i] = r.get<float>(at + 4 * size_t(i));
        out.boxMax[i] = r.get<float>(at + 12 + 4 * size_t(i));
    }
    out.cellSize[0] = r.get<float>(at + 0x18);
    out.cellSize[1] = r.get<float>(at + 0x1C);
    const uint32_t cells = r.get<uint32_t>(at + 0x20), lists = r.get<uint32_t>(at + 0x24);
    const uint32_t planeCount = r.get<uint32_t>(at + 0x28), planes = r.get<uint32_t>(at + 0x2C);
    const uint32_t polygonCount = r.get<uint32_t>(at + 0x30), polygons = r.get<uint32_t>(at + 0x34);
    if (!r.ok || !(out.cellSize[0] > 0) || !(out.cellSize[1] > 0) || planeCount > 0x7FFF || polygonCount > 0x7FFF)
        return false;
    for (uint32_t k = 0; k < planeCount && r.ok; k++)
        out.planes.push_back(readPiece(r, planes + kPlaneSize * k, false));
    for (uint32_t k = 0; k < polygonCount && r.ok; k++)
        out.polygons.push_back(readPiece(r, polygons + kPolygonSize * k, true));
    // The groups (surfaces): on at first when bit 0x01 of their type is set
    const uint32_t groupCount = r.get<uint32_t>(at + 0x38), groups = r.get<uint32_t>(at + 0x3C);
    for (uint32_t k = 0; k < groupCount && k < 0x10000 && r.ok; k++) {
        out.groupOn.push_back((r.get<uint16_t>(groups + 0x14 * size_t(k)) & 1) != 0 ? 1 : 0);
        out.groupFloor.push_back(int16_t(r.get<uint16_t>(groups + 0x14 * size_t(k) + 2)));
    }

    // The lists of the cells, copied one after the other
    out.cells.assign(kCollisionCells * kCollisionCells, kEmpty);
    for (size_t c = 0; c < out.cells.size() && r.ok; c++) {
        const uint16_t start = r.get<uint16_t>(cells + 2 * c);
        if (start & kEmpty)
            continue;
        out.cells[c] = uint16_t(out.lists.size());
        for (size_t k = start;; k++) {
            const uint16_t number = r.get<uint16_t>(lists + 2 * k);
            if (!r.ok || number == kEnd)
                break;
            const bool valid = (number & kPlaneBit) ? uint32_t(number & 0x7FFF) < planeCount : number < polygonCount;
            if (!valid)
                return false;
            out.lists.push_back(number);
        }
        out.lists.push_back(kEnd);
        if (out.lists.size() >= kEmpty)
            return false;
    }
    return r.ok;
}

void setCollisionGroup(CollisionMap& map, int group, bool on)
{
    if (group >= 0 && size_t(group) < map.groupOn.size())
        map.groupOn[size_t(group)] = on ? 1 : 0;
}

bool moveSphere(const CollisionMap& map, const float from[3], const float to[3], float radius, float out[3])
{
    Hit hit;
    if (!sweep(map, from, to, radius, hit)) {
        std::memcpy(out, to, sizeof(float) * 3);
        return false;
    }
    // Slide: from where it bumped, a little in front of the piece, the rest of
    // the move without the part that goes into it
    float rest[3], target[3];
    for (int i = 0; i < 3; i++)
        rest[i] = to[i] - hit.centre[i];
    const float into = rest[0] * hit.normal[0] + rest[1] * hit.normal[1] + rest[2] * hit.normal[2];
    for (int i = 0; i < 3; i++)
        target[i] = hit.centre[i] + kStandOff * hit.normal[i] + rest[i] - into * hit.normal[i];
    Hit again;
    if (sweep(map, from, target, radius, again))
        std::memcpy(out, from, sizeof(float) * 3);
    else
        std::memcpy(out, target, sizeof(float) * 3);
    return true;
}

bool floorBelow(const CollisionMap& map, const float point[3], float& floorZ, int* floorKind)
{
    const float x = (point[0] - map.boxMin[0]) / map.cellSize[0], y = (point[1] - map.boxMin[1]) / map.cellSize[1];
    if (map.cells.empty() || x < 0 || y < 0 || x >= kCollisionCells || y >= kCollisionCells)
        return false;
    bool found = false;
    forEachPiece(map, int(y) * kCollisionCells + int(x), [&](const CollisionPiece& piece) {
        if (!(piece.flags & kPieceFloor))
            return;
        // Straight down onto its plane (FUN_00405bec): a sloping floor, or a
        // level one (kind 2) under the point
        float z;
        if (piece.plane.kind == 2) {
            if (point[2] < piece.plane.d)
                return;
            z = piece.plane.d;
        } else if (piece.plane.kind == -1 && piece.plane.normal[2] > 0) {
            const float above = planeDistance(piece.plane, point);
            if (above < 0)
                return;
            z = point[2] - above / piece.plane.normal[2];
        } else
            return;
        const float on[3] = { point[0], point[1], z };
        if (!insideEdges(piece, on))
            return;
        z += float(piece.height);
        if (!found || z > floorZ) {
            floorZ = z;
            found = true;
            if (floorKind)
                *floorKind = piece.group >= 0 && size_t(piece.group) < map.groupFloor.size()
                                 ? map.groupFloor[size_t(piece.group)]
                                 : -1;
        }
    });
    return found;
}
