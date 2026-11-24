#pragma once

// The collision map of a level: the walls and floors the characters bump
// into and stand on, in world coordinates (centimetres, z up), in a grid of
// 16 x 16 cells over the floor (docs/FORMATOS.md; FUN_00401d4a, FUN_00401b4f).

#include <cstddef>
#include <cstdint>
#include <vector>

// A plane: the distance of a point to it is normal . point - d
struct CollisionPlane {
    int32_t kind = -1;  // -1 any, 0 x fixed, 1 y fixed, 2 z fixed (a level floor)
    float normal[3] = {};
    float d = 0;
};

// A piece of the map: a plane, cut by its edges if it has any (a polygon: a
// point is inside when its distance to every edge is 0 or more)
struct CollisionPiece {
    int16_t group = 0;       // its group: a piece of a group turned off does not count
    uint8_t flags = 0;       // 0x02: can be stood on
    uint8_t height = 0;      // added to the floor found on it
    CollisionPlane plane;
    std::vector<CollisionPlane> edges;
};

constexpr uint8_t kPieceFloor = 0x02;
constexpr int kCollisionCells = 16;

struct CollisionMap {
    float boxMin[3] = {}, boxMax[3] = {};
    float cellSize[2] = {};
    // Per cell (row after row, y x 16 + x) where its list starts in lists,
    // 0x8000 empty. A list: piece numbers up to 0xFFFF; with 0x8000 a plane
    // (number & 0x7FFF), without it a polygon
    std::vector<uint16_t> cells, lists;
    std::vector<CollisionPiece> planes, polygons;
    // Per group (the map's surfaces, 0x14 bytes each: type, kind of floor for
    // the footsteps, name), whether its pieces count: at first, bit 0x01 of
    // its type (setCollisionMap, FUN_00401021); scripts turn them on and off
    // (function 36, FUN_00401135): doors, gates...
    std::vector<uint8_t> groupOn;
    // Per group, its kind of floor for the footsteps (+2 of its 0x14 bytes;
    // -1 none): the table of sounds of the level it uses (DRAM +0x3DC,
    // FUN_0042f0ab)
    std::vector<int16_t> groupFloor;
};

// Turns a group of the map on or off (nothing for a group it has not)
void setCollisionGroup(CollisionMap& map, int group, bool on);

// The distance of a point to a plane (FUN_0040597d): more than 0 in front
float planeDistance(const CollisionPlane& plane, const float point[3]);

// Reads a plane of 0x14 bytes (kind, normal, d) at a place of a block. False
// if it falls outside the block
bool readCollisionPlane(const uint8_t* block, size_t size, size_t at, CollisionPlane& out);

// Reads the collision map at a place of a DRAM block (its pointers are
// places in the block). False if it is not what is expected
bool readCollisionMap(const uint8_t* block, size_t size, uint32_t at, CollisionMap& out);

// Moves a sphere (its centre) from one point to another through the map, as
// the characters move (FUN_004029ee): if it bumps into a piece it slides
// along it, and if it would still bump it stays where it was. out: where it
// ends. True if it bumped
bool moveSphere(const CollisionMap& map, const float from[3], const float to[3], float radius, float out[3]);

// The floor under a point: the highest piece that can be stood on in its cell
// that a line straight down from the point meets, plus that piece's height
// (FUN_004032c2). False if there is none
bool floorBelow(const CollisionMap& map, const float point[3], float& floorZ, int* floorKind = nullptr);
