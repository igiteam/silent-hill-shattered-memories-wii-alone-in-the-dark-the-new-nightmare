#pragma once

// The trigger map of a level (+0x3E8 of the DRAM; docs/FORMATOS.md): invisible
// walls that tell when the character's step crosses them (FUN_004a33fb). The
// level's pass triggers (+0x3AC) say what each crossing does: in level 1,
// change the camera. And zones (volumes) that tell when the character goes in
// and out of them (FUN_004a372c); the level's zone triggers (+0x3B0) say what
// that does.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "collisionMap.h"

// The step is tested this high over the feet (FUN_004a32c0(100))
constexpr float kTriggerHeight = 100.0f;

// A wall: a plane cut by up to 4 edges, like a polygon of the collision map
struct TriggerWall {
    CollisionPlane plane;
    std::vector<CollisionPlane> edges;
};

// A zone: inside when in front of all its planes (or on them)
struct TriggerZone {
    std::vector<CollisionPlane> planes;
};

struct TriggerMap {
    std::vector<TriggerWall> walls;
    std::vector<TriggerZone> zones;
};

// A wall crossed by a step: entering is from its front to its back
struct TriggerCrossing {
    int wall;
    bool entering;
};

// Reads the trigger map at a place of a DRAM block. False if it is not what
// is expected
bool readTriggerMap(const uint8_t* block, size_t size, uint32_t at, TriggerMap& out);

// Whether a point is inside a zone of the map
bool insideZone(const TriggerZone& zone, const float point[3]);

// The walls a step from one point to another crosses (FUN_004a36a7,
// FUN_00405f03), in the order of the map
void crossedWalls(const TriggerMap& map, const float from[3], const float to[3], std::vector<TriggerCrossing>& out);
