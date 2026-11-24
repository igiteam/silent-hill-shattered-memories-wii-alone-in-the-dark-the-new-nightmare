#include "viewScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>

#include "dataFile.h"

namespace {

// Places in the scene block (offsets from the start of the scene)
constexpr size_t kScenePosition = 0x04;
constexpr size_t kSceneAngles = 0x10;
constexpr size_t kSceneFov = 0x1C;
constexpr size_t kSceneCounts = 0x48;  // polygons, indices, points
constexpr size_t kSceneTables = 0x94;  // where they are in the block
constexpr size_t kPointSize = 0x24;    // x, y, z, 3 floats, u, v, 1 float

uint32_t readU32(const uint8_t* p)
{
    uint32_t value;
    std::memcpy(&value, p, 4);
    return value;
}

float readFloat(const uint8_t* p)
{
    float value;
    std::memcpy(&value, p, 4);
    return value;
}

// 3x3 matrices, row after row
struct Matrix3 {
    float m[3][3];
};

Matrix3 multiply(const Matrix3& a, const Matrix3& b)
{
    Matrix3 r = {};
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++)
                r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}

Matrix3 rotation(int axis, float angle)
{
    const float c = std::cos(angle), s = std::sin(angle);
    switch (axis) {
    case 0: return { { { 1, 0, 0 }, { 0, c, -s }, { 0, s, c } } };
    case 1: return { { { c, 0, s }, { 0, 1, 0 }, { -s, 0, c } } };
    default: return { { { c, -s, 0 }, { s, c, 0 }, { 0, 0, 1 } } };
    }
}

void subtract(const float a[3], const float b[3], float out[3])
{
    for (int i = 0; i < 3; i++)
        out[i] = a[i] - b[i];
}

void cross(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

float dot(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// Height along up of the floor of a scene: the level (in steps of 10) where
// most of the area of the mesh facing up is. The camera's height in the world
// is not enough: some rooms do not have their floor at 0
float floorHeight(const ViewScene& scene)
{
    std::map<int, std::pair<float, float>> levels; // level: area, area x height
    const std::vector<uint32_t> triangles = sceneTriangles(scene);
    for (size_t k = 0; k + 2 < triangles.size(); k += 3) {
        const ScenePoint& a = scene.points[triangles[k]];
        const ScenePoint& b = scene.points[triangles[k + 1]];
        const ScenePoint& c = scene.points[triangles[k + 2]];
        const float pa[3] = { a.x, a.y, a.z }, pb[3] = { b.x, b.y, b.z }, pc[3] = { c.x, c.y, c.z };
        float edge1[3], edge2[3], normal[3];
        subtract(pb, pa, edge1);
        subtract(pc, pa, edge2);
        cross(edge1, edge2, normal);
        const float area = std::sqrt(dot(normal, normal)) / 2;
        if (area <= 0.0f || std::fabs(dot(normal, scene.up)) < 0.8f * 2 * area)
            continue;
        const float height = (dot(pa, scene.up) + dot(pb, scene.up) + dot(pc, scene.up)) / 3;
        std::pair<float, float>& level = levels[int(std::floor(height / 10))];
        level.first += area;
        level.second += area * height;
    }
    const std::pair<float, float>* best = nullptr;
    for (const auto& level : levels)
        if (!best || level.second.first > best->first)
            best = &level.second;
    return best ? best->second / best->first : 0.0f;
}

} // namespace

bool loadViewScene(const std::string& dataFolder, int level, int camera, ViewScene& out)
{
    char name[64];
    std::snprintf(name, sizeof(name), "view_%02d", camera);
    std::filesystem::path path = std::filesystem::path(dataFolder) / "levels" / std::to_string(level) / name;
    std::vector<uint8_t> data;
    if (!readDataFile(path.string(), data) || data.size() < 16 || std::memcmp(data.data(), "G3DB", 4) != 0)
        return false;

    // G3DB: slot, size of the block, number of pointers, the block, the list
    // of pointers. Pointers are offsets from the start of the block
    const uint32_t size = readU32(&data[8]);
    const uint32_t pointers = readU32(&data[12]);
    if (data.size() != 16 + size_t(size) + 4 * size_t(pointers) || size < 0x14)
        return false;
    const uint8_t* block = &data[16];
    const uint32_t scene = readU32(block + 0x10);
    if (scene + kSceneTables + 12 > size)
        return false;
    const uint8_t* s = block + scene;

    for (int i = 0; i < 3; i++) {
        out.position[i] = readFloat(s + kScenePosition + 4 * i);
        out.angles[i] = readFloat(s + kSceneAngles + 4 * i);
    }
    out.fov = readFloat(s + kSceneFov);
    for (int k = 0; k < 2; k++) {
        const uint8_t* l = s + 0x20 + 0x14 * size_t(k);
        for (int i = 0; i < 3; i++)
            out.lights[k].place[i] = readFloat(l + 4 * i);
        out.lights[k].strength = float(readU32(l + 12));
        out.lights[k].height = readFloat(l + 16);
    }
    out.focal = (kViewWidth / 2) / std::tan(out.fov / 2);

    const uint32_t polygonCount = readU32(s + kSceneCounts);
    const uint32_t indexCount = readU32(s + kSceneCounts + 4);
    const uint32_t pointCount = readU32(s + kSceneCounts + 8);
    const uint32_t polygonTable = readU32(s + kSceneTables);
    const uint32_t indexTable = readU32(s + kSceneTables + 4);
    const uint32_t pointTable = readU32(s + kSceneTables + 8);
    if (size_t(polygonTable) + 12 * size_t(polygonCount) > size || size_t(indexTable) + 4 * size_t(indexCount) > size
        || size_t(pointTable) + kPointSize * pointCount > size)
        return false;

    out.points.resize(pointCount);
    for (uint32_t k = 0; k < pointCount; k++) {
        const uint8_t* p = block + pointTable + kPointSize * k;
        out.points[k] = { readFloat(p), readFloat(p + 4), readFloat(p + 8), readFloat(p + 0x18), readFloat(p + 0x1C) };
    }
    out.indices.resize(indexCount);
    for (uint32_t k = 0; k < indexCount; k++) {
        out.indices[k] = readU32(block + indexTable + 4 * k);
        if (out.indices[k] >= pointCount)
            return false;
    }
    out.polygons.resize(polygonCount);
    for (uint32_t k = 0; k < polygonCount; k++) {
        const uint8_t* p = block + polygonTable + 12 * k;
        out.polygons[k] = { readU32(p), readU32(p + 4), readU32(p + 8) };
        if (size_t(out.polygons[k].first) + out.polygons[k].count > indexCount)
            return false;
    }

    // World from camera space: rotation about x, then z, then y (checked
    // against the floor of the meshes, docs/FORMATOS.md). The world's up in
    // camera space is the third row of that rotation
    const Matrix3 world = multiply(multiply(rotation(0, out.angles[0]), rotation(2, out.angles[2])),
                                   rotation(1, out.angles[1]));
    for (int i = 0; i < 3; i++) {
        out.up[i] = world.m[2][i];
        for (int j = 0; j < 3; j++)
            out.toWorld[i][j] = world.m[i][j];
    }
    out.floor = floorHeight(out);
    return true;
}

void cameraToWorld(const ViewScene& scene, const float point[3], float out[3])
{
    for (int i = 0; i < 3; i++)
        out[i] = scene.toWorld[i][0] * point[0] + scene.toWorld[i][1] * point[1] + scene.toWorld[i][2] * point[2]
                 + scene.position[i];
}

void worldToCamera(const ViewScene& scene, const float point[3], float out[3])
{
    const float p[3] = { point[0] - scene.position[0], point[1] - scene.position[1], point[2] - scene.position[2] };
    for (int i = 0; i < 3; i++)
        out[i] = scene.toWorld[0][i] * p[0] + scene.toWorld[1][i] * p[1] + scene.toWorld[2][i] * p[2];
}

void worldDirectionToCamera(const ViewScene& scene, const float direction[3], float out[3])
{
    for (int i = 0; i < 3; i++)
        out[i] = scene.toWorld[0][i] * direction[0] + scene.toWorld[1][i] * direction[1]
                 + scene.toWorld[2][i] * direction[2];
}

void projectToPicture(const ViewScene& scene, const float point[3], float& x, float& y)
{
    x = scene.focal * point[0] / point[1] + kViewWidth / 2;
    y = kViewHeight / 2 - scene.focal * point[2] / point[1];
}

std::vector<uint32_t> sceneTriangles(const ViewScene& scene, size_t* roomCount)
{
    std::vector<uint32_t> triangles;
    for (int objects = 0; objects < 2; objects++) {
        for (const ScenePolygon& strip : scene.polygons) {
            if ((strip.group != 0) != (objects != 0))
                continue;
            for (uint32_t k = 2; k < strip.count; k++) {
                // Every other triangle of a strip is turned the other way round
                const uint32_t* p = &scene.indices[strip.first + k - 2];
                triangles.push_back(p[0]);
                triangles.push_back(k % 2 == 0 ? p[1] : p[2]);
                triangles.push_back(k % 2 == 0 ? p[2] : p[1]);
            }
        }
        if (objects == 0 && roomCount)
            *roomCount = triangles.size();
    }
    return triangles;
}

bool pickFloor(const ViewScene& scene, float pictureX, float pictureY, float out[3])
{
    const float direction[3] = { (pictureX - kViewWidth / 2) / scene.focal, 1.0f,
                                 (kViewHeight / 2 - pictureY) / scene.focal };
    const float down = dot(direction, scene.up);
    if (down >= 0.0f)
        return false;
    const float distance = scene.floor / down;
    for (int i = 0; i < 3; i++)
        out[i] = direction[i] * distance;
    return true;
}

std::vector<uint8_t> foregroundMask(const ViewSheet& dark)
{
    std::vector<uint8_t> mask(dark.pixels.size());
    for (size_t i = 0; i < mask.size(); i++)
        mask[i] = (dark.pixels[i] & kBackgroundBit) ? 0 : 255;
    return mask;
}
