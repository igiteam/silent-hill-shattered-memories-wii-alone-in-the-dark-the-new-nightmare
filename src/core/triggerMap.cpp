#include "triggerMap.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr size_t kWallSize = 0x68, kEdgeSize = 0x14;
constexpr uint32_t kMaxEdges = 4;

uint32_t readU32(const uint8_t* p)
{
    uint32_t value;
    std::memcpy(&value, p, 4);
    return value;
}

int16_t readI16(const uint8_t* p)
{
    int16_t value;
    std::memcpy(&value, p, 2);
    return value;
}

} // namespace

bool readTriggerMap(const uint8_t* block, size_t size, uint32_t at, TriggerMap& out)
{
    out = TriggerMap{};
    if (at == 0 || size_t(at) + 0x18 > size)
        return false;
    const int16_t wallCount = readI16(block + at + 4), zoneCount = readI16(block + at + 6);
    const uint32_t walls = readU32(block + at + 0x10), zones = readU32(block + at + 0x14);
    if (wallCount < 0 || size_t(walls) + kWallSize * size_t(wallCount) > size)
        return false;
    for (int16_t k = 0; k < wallCount; k++) {
        const size_t wall = walls + kWallSize * size_t(k);
        TriggerWall w;
        if (!readCollisionPlane(block, size, wall, w.plane))
            return false;
        const uint32_t edges = std::min(readU32(block + wall + 0x14), kMaxEdges);
        w.edges.resize(edges);
        for (uint32_t e = 0; e < edges; e++)
            if (!readCollisionPlane(block, size, wall + 0x18 + kEdgeSize * e, w.edges[e]))
                return false;
        out.walls.push_back(std::move(w));
    }
    // Zones (8 bytes each): number of planes and where they are
    if (zoneCount < 0 || size_t(zones) + 8 * size_t(zoneCount) > size)
        return false;
    for (int16_t k = 0; k < zoneCount; k++) {
        const uint32_t count = readU32(block + zones + 8 * size_t(k)), planes = readU32(block + zones + 8 * size_t(k) + 4);
        if (count > 64)
            return false;
        TriggerZone zone;
        zone.planes.resize(count);
        for (uint32_t p = 0; p < count; p++)
            if (!readCollisionPlane(block, size, planes + kEdgeSize * p, zone.planes[p]))
                return false;
        out.zones.push_back(std::move(zone));
    }
    return true;
}

bool insideZone(const TriggerZone& zone, const float point[3])
{
    for (const CollisionPlane& plane : zone.planes)
        if (planeDistance(plane, point) < 0)
            return false;
    return true;
}

void crossedWalls(const TriggerMap& map, const float from[3], const float to[3], std::vector<TriggerCrossing>& out)
{
    out.clear();
    for (size_t k = 0; k < map.walls.size(); k++) {
        const TriggerWall& wall = map.walls[k];
        // The step goes from one side to the other (one end may touch it)
        const float before = planeDistance(wall.plane, from), after = planeDistance(wall.plane, to);
        if (!(before > 0 || after > 0) || !(before <= 0 || after <= 0))
            continue;
        const float t = before / (before - after);
        float point[3];
        for (int i = 0; i < 3; i++)
            point[i] = from[i] + (to[i] - from[i]) * t;
        bool inside = true;
        for (const CollisionPlane& edge : wall.edges)
            if (planeDistance(edge, point) < 0)
                inside = false;
        if (inside)
            out.push_back({ int(k), before >= 0 });
    }
}
