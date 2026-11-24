#pragma once

// Scene of a camera: levels\<level>\view_NN, a G3DB chunk (docs/FORMATOS.md).
// The camera and a 3D mesh of the room laid exactly over its background: the
// game lights it with the flashlight, and its pixels without the background
// bit hide what is behind them (the foreground).

#include <cstdint>
#include <string>
#include <vector>

#include "viewPicture.h"

// A point of the mesh, in camera space: x right, y depth, z up (camera up,
// not the world's). u, v: where it is on the dark sheet (0..1 of 1024)
struct ScenePoint {
    float x, y, z;
    float u, v;
};

// A strip of triangles: indices[first .. first + count) are its points, each
// triangle made of a point and the two before it. Group 0: the room; 1 and
// more: objects of the foreground
struct ScenePolygon {
    uint32_t group, first, count;
};

struct ViewScene {
    float position[3] = {};  // of the camera in the world (z up, floor near 0)
    float angles[3] = {};    // of the camera, radians
    float fov = 0;           // horizontal angle of view, radians
    float focal = 0;         // the same, in pixels of the 640x480 picture
    float up[3] = {};        // the world's up, in camera space
    // World from camera space: world = toWorld x point + position; toWorld =
    // Rx(angles[0]) Rz(angles[2]) Ry(angles[1]) (the walls of the meshes fall
    // on those of the level's collision map with it)
    float toWorld[3][3] = {};
    float floor = 0;         // height of the floor along up (negative: below the camera)
    // The scene's two lights ("LightMesh" +0x20, +0x34; FUN_0041bd50): where
    // (world, high up: the moon), how strong (50..180 of 255) and a height
    // for the shadows (FUN_00423dc0); the second one when the level is lit
    struct Light {
        float place[3] = {};
        float strength = 0;
        float height = 0;
    };
    Light lights[2];
    std::vector<ScenePolygon> polygons;
    std::vector<uint32_t> indices;
    std::vector<ScenePoint> points;
};

// Reads the scene of a camera of a level. False if the file is missing or is
// not what is expected
bool loadViewScene(const std::string& dataFolder, int level, int camera, ViewScene& out);

// A point of camera space in the world, and back
void cameraToWorld(const ViewScene& scene, const float point[3], float out[3]);
void worldToCamera(const ViewScene& scene, const float point[3], float out[3]);

// A direction of the world in camera space (turned, not moved)
void worldDirectionToCamera(const ViewScene& scene, const float direction[3], float out[3]);

// Where a point of camera space falls in the 640x480 picture, in pixels
void projectToPicture(const ViewScene& scene, const float point[3], float& x, float& y);

// The strips of the scene cut into triangles: three point numbers each.
// First those of the room (group 0), then those of the objects of the
// foreground (groups 1 and more: big flat pieces whose sheet's mask says
// where the object really is); roomCount: how many numbers are the room's
std::vector<uint32_t> sceneTriangles(const ViewScene& scene, size_t* roomCount = nullptr);

// The point of the floor seen at a pixel of the picture (the floor is taken
// as flat, at the height of most of the mesh facing up: the mesh is not a whole
// room, it has holes). False if that pixel looks above the horizon
bool pickFloor(const ViewScene& scene, float pictureX, float pictureY, float out[3]);

// The mask of the foreground for the mesh, from the dark sheet: one byte a
// pixel, 255 where the pixel belongs to an object of the foreground, 0 where
// it is background (bit 0x80). The game also fills a lit copy of the sheet
// for the mesh (FUN_004061b0); the viewer takes the colours from the picture
// at the same place on the screen instead, which is the same and has no seams
std::vector<uint8_t> foregroundMask(const ViewSheet& dark);
