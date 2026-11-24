#include "skeletonModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>

#include "dataFile.h"

namespace {

constexpr size_t kSkeletonSize = 0x70;  // one model in the bank
constexpr size_t kBoneSize = 0x24;
constexpr size_t kVertexSize = 0x24;
constexpr size_t kTextureSize = 0x30;
constexpr size_t kTrackSize = 0x14;
constexpr size_t kAnimationSize = 0x30;
constexpr size_t kKeySize = 0x10;
constexpr float kPi = 3.14159265358979f;

// Reads from a block, failing (and leaving ok false) outside it
struct Reader {
    const uint8_t* data;
    size_t size;
    bool ok = true;

    uint32_t u32(size_t at)
    {
        if (at + 4 > size) {
            ok = false;
            return 0;
        }
        uint32_t value;
        std::memcpy(&value, data + at, 4);
        return value;
    }
    float f32(size_t at)
    {
        const uint32_t bits = u32(at);
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    uint16_t u16(size_t at)
    {
        if (at + 2 > size) {
            ok = false;
            return 0;
        }
        return uint16_t(data[at] | (data[at + 1] << 8));
    }
};

// Place of the pixel (x, y) in a square texture: the bits of x and y
// interleaved, y first (Z order, as on the Dreamcast)
uint32_t twiddled(uint32_t x, uint32_t y)
{
    uint32_t place = 0;
    for (int bit = 0; bit < 16; bit++)
        place |= (((y >> bit) & 1u) << (2 * bit)) | (((x >> bit) & 1u) << (2 * bit + 1));
    return place;
}

// A texture: 16 bits a pixel; flag 0x01 RGB 5-6-5, otherwise ARGB 1-5-5-5.
// Flag 0x04: compressed as the Dreamcast's VQ (G3D_ResourceBank_UploadChunk
// gives it the format 0x300, which FUN_0048e037 unpacks): a book of 256
// blocks of 2 x 2 pixels (in Z order), then a byte a block of the picture
// (in Z order) saying which
bool readTexture(Reader& r, size_t at, ModelTexture& out)
{
    const uint32_t width = r.u16(at), height = r.u16(at + 2), flags = r.u32(at + 4);
    const uint32_t pixels = r.u32(at + 0x28);
    const bool vq = (flags & 4) != 0;
    const size_t bytes = vq ? 256 * 8 + size_t(width / 2) * (height / 2) : size_t(width) * height * 2;
    if (!r.ok || width != height || width < 2 || width > 1024 || pixels + bytes > r.size)
        return false;
    out.size = int(width);
    out.rgba.resize(size_t(width) * height);
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t p;
            if (vq) {
                const uint32_t block = r.data[pixels + 256 * 8 + twiddled(x / 2, y / 2)];
                p = r.u16(pixels + size_t(block) * 8 + size_t(twiddled(x & 1, y & 1)) * 2);
            } else
                p = r.u16(pixels + size_t(twiddled(x, y)) * 2);
            uint32_t red, green, blue, alpha;
            if (flags & 1) {
                red = (p >> 11) & 31;
                green = ((p >> 5) & 63) * 31 / 63;
                blue = p & 31;
                alpha = 255;
            } else {
                red = (p >> 10) & 31;
                green = (p >> 5) & 31;
                blue = p & 31;
                alpha = (p >> 15) ? 255 : 0;
            }
            out.rgba[size_t(y) * width + x] = (red * 255 / 31) | ((green * 255 / 31) << 8) | ((blue * 255 / 31) << 16)
                                              | (alpha << 24);
        }
    }
    return true;
}

bool readKeys(Reader& r, size_t at, uint32_t count, std::vector<AnimationKey>& out)
{
    out.resize(count);
    for (uint32_t k = 0; k < count && r.ok; k++)
        out[k] = { { r.f32(at + kKeySize * k), r.f32(at + kKeySize * k + 4), r.f32(at + kKeySize * k + 8) },
                   r.f32(at + kKeySize * k + 12) };
    return r.ok;
}

// The animations of a model: at +0x6c of the model, the number of tracks and
// of animations (u16 each) and where they are. 0: the model has none
bool readAnimations(Reader& r, uint32_t set, uint32_t modelTable, SkeletonModel& out)
{
    if (set == 0)
        return true;
    const uint32_t trackCount = r.u16(set), animationCount = r.u16(set + 2);
    const uint32_t tracks = r.u32(set + 4), animations = r.u32(set + 8), identifiers = r.u32(set + 12);
    if (!r.ok)
        return false;
    out.tracks.resize(trackCount);
    for (uint32_t t = 0; t < trackCount && r.ok; t++) {
        const size_t p = tracks + kTrackSize * t;
        AnimationTrack& track = out.tracks[t];
        track.bone = r.u16(p + 6);
        if (!readKeys(r, r.u32(p + 8), r.u16(p), track.positions) || !readKeys(r, r.u32(p + 12), r.u16(p + 2), track.rotations)
            || !readKeys(r, r.u32(p + 16), r.u16(p + 4), track.scales))
            return false;
    }
    out.animations.resize(animationCount);
    for (uint32_t a = 0; a < animationCount && r.ok; a++) {
        const size_t p = animations + kAnimationSize * a;
        ModelAnimation& animation = out.animations[a];
        animation.speed = r.f32(p);
        animation.length = r.u16(p + 4);
        animation.flags = r.u16(p + 6);
        animation.rate = r.u16(p + 0x14);
        // Transitions to other animations (0x10 bytes each, FUN_00427030):
        // speed while blending, its length, windows, target, list of windows
        const uint32_t transitionList = r.u32(p + 0x28);
        for (uint32_t k = 0; k < r.u16(p + 0x10) && r.ok; k++) {
            const size_t q = transitionList + 0x10 * size_t(k);
            AnimationTransition transition;
            transition.speed = r.f32(q);
            transition.duration = r.u16(q + 4);
            const uint32_t target = r.u32(q + 8);
            if (target >= animations && (target - animations) % kAnimationSize == 0
                && (target - animations) / kAnimationSize < animationCount)
                transition.target = int((target - animations) / kAnimationSize);
            const uint32_t windows = r.u32(q + 12);
            for (uint32_t w = 0; w < r.u16(q + 6) && r.ok; w++) {
                const size_t z = windows + 8 * size_t(w);
                transition.windows.push_back({ r.u16(z), r.u16(z + 2), r.u16(z + 4), r.u16(z + 6) });
            }
            animation.transitions.push_back(std::move(transition));
        }
        // What happens at moments of it (8 bytes each)
        const uint32_t eventList = r.u32(p + 0x2C);
        for (uint32_t k = 0; k < r.u16(p + 0x12) && r.ok; k++) {
            const size_t q = eventList + 8 * size_t(k);
            animation.events.push_back({ r.u16(q), r.u16(q + 2), r.u16(q + 4), r.u16(q + 6) });
        }
        auto trackList = [&](uint32_t list, uint32_t count, std::vector<uint32_t>& to) {
            for (uint32_t k = 0; k < count && r.ok; k++) {
                const uint32_t track = r.u16(list + 2 * k);
                if (track < trackCount)
                    to.push_back(track);
            }
        };
        trackList(r.u32(p + 0x18), r.u16(p + 8), animation.tracks);
        // Groups with their own clock (0x10 bytes): tracks, flags, length, list
        const uint32_t clocked = r.u32(p + 0x1C);
        for (uint32_t g = 0; g < r.u16(p + 0x0A) && r.ok; g++) {
            const size_t q = clocked + 0x10 * g;
            AnimationGroup group{ r.u16(q + 6), r.u16(q + 4), {} };
            trackList(r.u32(q + 8), r.u16(q), group.tracks);
            animation.clocked.push_back(std::move(group));
        }
        // Mixes (0x10 bytes): sources, flags, length, and a list: tracks per
        // source (i32), then per source its track list, weight slot and one more
        const uint32_t mixes = r.u32(p + 0x20);
        for (uint32_t g = 0; g < r.u16(p + 0x0C) && r.ok; g++) {
            const size_t q = mixes + 0x10 * g;
            AnimationMix mix{ r.u16(q + 6), r.u16(q + 4), {} };
            const uint32_t sources = r.u32(q + 8), perSource = r.u32(sources);
            for (uint32_t s = 0; s < r.u16(q) && r.ok && perSource < 256; s++) {
                AnimationMix::Source source{ {}, r.u32(sources + 4 + 12 * s + 4) };
                trackList(r.u32(sources + 4 + 12 * s), perSource, source.tracks);
                if (source.tracks.size() != perSource || source.weightSlot >= uint32_t(kMixSlots))
                    return false;
                mix.sources.push_back(std::move(source));
            }
            animation.mixes.push_back(std::move(mix));
        }
        // Fixed poses (0x14 bytes): tracks, list at +8, read at moment 0
        const uint32_t fixedPoses = r.u32(p + 0x24);
        for (uint32_t g = 0; g < r.u16(p + 0x0E) && r.ok; g++) {
            const size_t q = fixedPoses + 0x14 * g;
            AnimationGroup group{ 0, 0, {} };
            trackList(r.u32(q + 8), r.u16(q), group.tracks);
            animation.fixedPoses.push_back(std::move(group));
        }
    }
    // The identifiers: rows of 10 u16 up to the list of models of the bank
    const uint32_t end = identifiers < modelTable ? modelTable : uint32_t(r.size);
    for (uint32_t at = identifiers; at + 20 <= end && r.ok; at += 20) {
        std::array<int16_t, 10> row;
        for (int k = 0; k < 10; k++)
            row[size_t(k)] = int16_t(r.u16(at + 2 * size_t(k)));
        out.identifiers.push_back(row);
    }
    return r.ok;
}

// The value of a track's keys at a moment (FUN_00413130; angles: FUN_004132e8,
// which goes the short way round). False if there are no keys
bool sampleKeys(const std::vector<AnimationKey>& keys, float time, bool angles, float out[3])
{
    if (keys.empty())
        return false;
    for (size_t k = 0; k < keys.size(); k++) {
        if (angles && time == keys[k].time) {
            std::memcpy(out, keys[k].value, sizeof(float) * 3);
            return true;
        }
        if (time < keys[k].time) {
            if (k == 0) {
                std::memcpy(out, keys[0].value, sizeof(float) * 3);
                return true;
            }
            const AnimationKey& before = keys[k - 1];
            const float f = (time - before.time) / (keys[k].time - before.time);
            for (int i = 0; i < 3; i++) {
                float change = keys[k].value[i] - before.value[i];
                if (angles)
                    change = std::remainder(change, 2 * kPi);
                out[i] = before.value[i] + change * f;
            }
            return true;
        }
    }
    std::memcpy(out, keys.back().value, sizeof(float) * 3);
    return true;
}

BoneMatrix multiply(const BoneMatrix& a, const BoneMatrix& b)
{
    BoneMatrix r = {};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}

// Turns the columns a and b of every row by an angle, as FUN_00426ab0 does
void turnColumns(BoneMatrix& m, int a, int b, float angle, bool aFirst)
{
    const float c = std::cos(angle), s = std::sin(angle);
    for (auto& row : m.m) {
        const float x = row[a], y = row[b];
        if (aFirst) {
            row[a] = c * x - s * y;
            row[b] = s * x + c * y;
        } else {
            row[a] = c * x + s * y;
            row[b] = c * y - s * x;
        }
    }
}

bool readModel(Reader& r, size_t at, uint32_t modelTable, SkeletonModel& out)
{
    uint32_t e[14];
    for (int i = 0; i < 14; i++)
        e[i] = r.u32(at + 4 * i);
    const uint32_t boneCount = e[0], stripCount = e[1], indexCount = e[2], vertexCount = e[3];
    const uint32_t materialCount = e[4], textureCount = e[5];
    const uint32_t bones = e[6], strips = e[9], indices = e[10], vertices = e[11], materials = e[12], textures = e[13];
    if (!r.ok || boneCount > 256 || materialCount > 64 || textureCount > 64)
        return false;

    out.bones.resize(boneCount);
    for (uint32_t b = 0; b < boneCount; b++) {
        ModelBone& bone = out.bones[b];
        const size_t p = bones + kBoneSize * b;
        for (int i = 0; i < 3; i++) {
            bone.offset[i] = r.f32(p + 4 * i);
            bone.rest[i] = r.f32(p + 12 + 4 * i);
        }
        bone.parent = int(r.u32(p + 0x18));
        bone.firstVertex = r.u32(p + 0x1C);
        bone.endVertex = r.u32(p + 0x20);
    }
    out.vertices.resize(vertexCount);
    for (uint32_t v = 0; v < vertexCount && r.ok; v++) {
        const size_t p = vertices + kVertexSize * v;
        out.vertices[v] = { r.f32(p), r.f32(p + 4), r.f32(p + 8), r.f32(p + 12), r.f32(p + 16), r.f32(p + 20),
                            r.f32(p + 24), r.f32(p + 28) };
    }
    out.materials.resize(materialCount);
    for (uint32_t m = 0; m < materialCount; m++) {
        out.materials[m].texture = int(r.u32(materials + 8 * m));
        out.materials[m].flags = r.u32(materials + 8 * m + 4);
    }
    // Strips (material, first index, number of indices), cut into triangles
    // the way the game does (FUN_00425320)
    for (uint32_t s = 0; s < stripCount && r.ok; s++) {
        const uint32_t material = r.u32(strips + 12 * s), first = r.u32(strips + 12 * s + 4);
        const uint32_t length = r.u32(strips + 12 * s + 8);
        if (material >= materialCount || first + size_t(length) > indexCount)
            return false;
        uint32_t a = r.u32(indices + 4 * size_t(first)), b = r.u32(indices + 4 * size_t(first + 1));
        bool turn = false;
        for (uint32_t j = 2; j < length; j++) {
            const uint32_t c = r.u32(indices + 4 * size_t(first + j));
            if (a >= vertexCount || b >= vertexCount || c >= vertexCount)
                return false;
            std::vector<uint32_t>& triangles = out.materials[material].triangles;
            triangles.insert(triangles.end(), { a, b, c });
            if (turn)
                b = c;
            else
                a = c;
            turn = !turn;
        }
    }
    out.textures.resize(textureCount);
    for (uint32_t t = 0; t < textureCount; t++)
        if (!readTexture(r, textures + kTextureSize * t, out.textures[t]))
            return false;
    return r.ok && readAnimations(r, r.u32(at + 0x6C), modelTable, out);
}

} // namespace

AnimationClock advanceAnimation(const ModelAnimation& animation, float& time, float elapsed)
{
    const float length = float(animation.length);
    const bool loops = (animation.flags & kAnimationLoops) != 0;
    if (animation.flags & kAnimationBackwards) {
        time -= elapsed;
        if (time > 0)
            return AnimationClock::Running;
        if (!loops || length <= 0) {
            time = 0;
            return AnimationClock::Ended;
        }
        time = std::fmod(time, length) + length;
        return AnimationClock::Looped;
    }
    time += elapsed;
    if (time < length)
        return AnimationClock::Running;
    if (!loops || length <= 0) {
        time = length;
        return AnimationClock::Ended;
    }
    time = std::fmod(time, length);
    return AnimationClock::Looped;
}

bool rootMotion(const SkeletonModel& model, size_t animation, float time, float place[3], float angles[3])
{
    place[0] = place[1] = place[2] = 0;
    angles[0] = angles[1] = angles[2] = 0;
    if (animation >= model.animations.size() || model.animations[animation].tracks.empty())
        return false;
    const AnimationTrack& track = model.tracks[model.animations[animation].tracks[0]];
    if (track.bone != 0)
        return false;
    sampleKeys(track.positions, time, false, place);
    sampleKeys(track.rotations, time, true, angles);
    return true;
}

void poseSkeleton(const SkeletonModel& model, size_t animation, float time, std::vector<BoneMatrix>& out)
{
    float weights[kMixSlots] = { 0.25f, 0.25f, 0.25f, 0.25f };
    poseSkeleton(model, animation, time, weights, out);
}

void poseSkeleton(const SkeletonModel& model, size_t animation, float time, const float weights[kMixSlots],
                  std::vector<BoneMatrix>& out)
{
    BonePose pose;
    sampleBonePose(model, animation, time, weights, pose);
    buildSkeleton(model, pose, out);
}

void sampleBonePose(const SkeletonModel& model, size_t animation, float time, BonePose& out)
{
    float weights[kMixSlots] = { 0.25f, 0.25f, 0.25f, 0.25f };
    sampleBonePose(model, animation, time, weights, out);
}

void sampleBonePose(const SkeletonModel& model, size_t animation, float time, const float weights[kMixSlots],
                    BonePose& out)
{
    const size_t boneCount = model.bones.size();
    // What the animation says of each bone; the rest pose where it says nothing
    std::vector<std::array<float, 3>>& place = out.place;
    std::vector<std::array<float, 3>>& angles = out.angles;
    std::vector<std::array<float, 3>>& scale = out.scale;
    place.resize(boneCount);
    angles.assign(boneCount, { 0, 0, 0 });
    scale.assign(boneCount, { 1, 1, 1 });
    for (size_t b = 0; b < boneCount; b++)
        place[b] = { model.bones[b].offset[0], model.bones[b].offset[1], model.bones[b].offset[2] };
    auto sampleTracks = [&](const std::vector<uint32_t>& tracks, float at) {
        for (uint32_t t : tracks) {
            const AnimationTrack& track = model.tracks[t];
            if (track.bone == 0 || track.bone >= boneCount)
                continue;
            sampleKeys(track.positions, at, false, place[track.bone].data());
            sampleKeys(track.rotations, at, true, angles[track.bone].data());
            sampleKeys(track.scales, at, false, scale[track.bone].data());
        }
    };
    // A group's own moment: the animation's, over and over in its length
    auto groupTime = [&](uint32_t length) { return length > 0 ? std::fmod(time, float(length)) : time; };
    if (animation < model.animations.size()) {
        const ModelAnimation& a = model.animations[animation];
        sampleTracks(a.tracks, time);
        for (const AnimationGroup& group : a.clocked)
            sampleTracks(group.tracks, groupTime(group.length));
        // Mixes: per bone, the sum of every source's value by its weight
        // (a source without keys gives the rest place, no turn and scale 1)
        for (const AnimationMix& mix : a.mixes) {
            const float at = groupTime(mix.length);
            const size_t count = mix.sources.empty() ? 0 : mix.sources[0].tracks.size();
            for (size_t k = 0; k < count; k++) {
                const uint32_t bone = model.tracks[mix.sources[0].tracks[k]].bone;
                if (bone == 0 || bone >= boneCount)
                    continue;
                std::array<float, 3> sumPlace = { 0, 0, 0 }, sumAngles = { 0, 0, 0 }, sumScale = { 0, 0, 0 };
                for (const AnimationMix::Source& source : mix.sources) {
                    const AnimationTrack& track = model.tracks[source.tracks[k]];
                    const float w = weights[source.weightSlot];
                    float value[3];
                    if (!sampleKeys(track.positions, at, false, value))
                        std::memcpy(value, model.bones[bone].offset, sizeof(value));
                    for (int i = 0; i < 3; i++)
                        sumPlace[size_t(i)] += value[i] * w;
                    if (sampleKeys(track.rotations, at, true, value))
                        for (int i = 0; i < 3; i++)
                            sumAngles[size_t(i)] += value[i] * w;
                    if (!sampleKeys(track.scales, at, false, value))
                        value[0] = value[1] = value[2] = 1.0f;
                    for (int i = 0; i < 3; i++)
                        sumScale[size_t(i)] += value[i] * w;
                }
                place[bone] = sumPlace;
                angles[bone] = sumAngles;
                scale[bone] = sumScale;
            }
        }
        for (const AnimationGroup& group : a.fixedPoses)
            sampleTracks(group.tracks, 0.0f);
    }
}

void buildSkeleton(const SkeletonModel& model, const BonePose& pose, std::vector<BoneMatrix>& out)
{
    const size_t boneCount = std::min(model.bones.size(), pose.place.size());
    const std::vector<std::array<float, 3>>& place = pose.place;
    const std::vector<std::array<float, 3>>& angles = pose.angles;
    const std::vector<std::array<float, 3>>& scale = pose.scale;
    out.resize(boneCount);
    for (size_t b = 0; b < boneCount; b++) {
        BoneMatrix m = { { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } } };
        turnColumns(m, 0, 1, angles[b][2], true);   // about z
        turnColumns(m, 0, 2, angles[b][1], false);  // about y
        turnColumns(m, 1, 2, angles[b][0], true);   // about x
        for (int row = 0; row < 3; row++)
            for (int column = 0; column < 3; column++)
                m.m[row][column] *= scale[b][column];
        for (int i = 0; i < 3; i++)
            m.m[3][i] += place[b][i];
        const int parent = model.bones[b].parent;
        out[b] = (parent >= 0 && size_t(parent) < b) ? multiply(m, out[size_t(parent)]) : m;
    }
}

void blendBonePoses(const BonePose& from, const BonePose& to, float amount, BonePose& out)
{
    const size_t boneCount = std::min(from.place.size(), to.place.size());
    out.place.resize(boneCount);
    out.angles.resize(boneCount);
    out.scale.resize(boneCount);
    for (size_t b = 0; b < boneCount; b++)
        for (size_t i = 0; i < 3; i++) {
            out.place[b][i] = from.place[b][i] + (to.place[b][i] - from.place[b][i]) * amount;
            out.scale[b][i] = from.scale[b][i] + (to.scale[b][i] - from.scale[b][i]) * amount;
            // Angles the short way round, kept between -pi and pi (FUN_0040602e)
            const float turn = std::remainder(to.angles[b][i] - from.angles[b][i], 2 * kPi);
            out.angles[b][i] = std::remainder(from.angles[b][i] + turn * amount, 2 * kPi);
        }
}

const AnimationTransition* findTransition(const SkeletonModel& model, size_t from, size_t to)
{
    if (from >= model.animations.size())
        return nullptr;
    for (const AnimationTransition& transition : model.animations[from].transitions)
        if (transition.target >= 0 && size_t(transition.target) == to)
            return &transition;
    return nullptr;
}

namespace {

// A vertex moved by a bone: p is its place measured from the bone
void moveVertex(const BoneMatrix& m, const float p[3], const ModelVertex& from, ModelVertex& to)
{
    to.x = p[0] * m.m[0][0] + p[1] * m.m[1][0] + p[2] * m.m[2][0] + m.m[3][0];
    to.y = p[0] * m.m[0][1] + p[1] * m.m[1][1] + p[2] * m.m[2][1] + m.m[3][1];
    to.z = p[0] * m.m[0][2] + p[1] * m.m[1][2] + p[2] * m.m[2][2] + m.m[3][2];
    to.nx = from.nx * m.m[0][0] + from.ny * m.m[1][0] + from.nz * m.m[2][0];
    to.ny = from.nx * m.m[0][1] + from.ny * m.m[1][1] + from.nz * m.m[2][1];
    to.nz = from.nx * m.m[0][2] + from.ny * m.m[1][2] + from.nz * m.m[2][2];
}

} // namespace

void skinVertices(const SkeletonModel& model, const std::vector<BoneMatrix>& bones, std::vector<ModelVertex>& out)
{
    out = model.vertices;
    for (size_t b = 0; b < model.bones.size() && b < bones.size(); b++) {
        const ModelBone& bone = model.bones[b];
        for (uint32_t v = bone.firstVertex; v < bone.endVertex && v < out.size(); v++) {
            const ModelVertex& rest = model.vertices[v];
            const float p[3] = { rest.x - bone.rest[0], rest.y - bone.rest[1], rest.z - bone.rest[2] };
            moveVertex(bones[b], p, rest, out[v]);
        }
    }
}

void attachVertices(const SkeletonModel& part, const BoneMatrix& bone, std::vector<ModelVertex>& out)
{
    out = part.vertices;
    for (size_t v = 0; v < out.size(); v++) {
        const ModelVertex& rest = part.vertices[v];
        const float p[3] = { rest.x, rest.y, rest.z };
        moveVertex(bone, p, rest, out[v]);
    }
}

int animationWithIdentifier(const SkeletonModel& model, int identifier)
{
    if (identifier < 0 || size_t(identifier / 10) >= model.identifiers.size())
        return -1;
    const int animation = model.identifiers[size_t(identifier / 10)][size_t(identifier % 10)];
    return animation >= 0 && size_t(animation) < model.animations.size() ? animation : -1;
}

int wristBone(const SkeletonModel& model, bool left)
{
    int best = -1;
    for (size_t b = 0; b < model.bones.size(); b++) {
        const ModelBone& bone = model.bones[b];
        if (bone.parent < 0 || bone.endVertex > bone.firstVertex)
            continue;
        const float x = bone.rest[0];
        if ((left && x < 0 && (best < 0 || x < model.bones[size_t(best)].rest[0]))
            || (!left && x > 0 && (best < 0 || x > model.bones[size_t(best)].rest[0])))
            best = int(b);
    }
    return best;
}

namespace {

// The models of the G3DB chunk at bank in data
bool readBank(const std::vector<uint8_t>& data, size_t bank, std::vector<SkeletonModel>& out)
{
    if (bank + 16 > data.size() || std::memcmp(&data[bank], "G3DB", 4) != 0)
        return false;
    Reader file{ data.data(), data.size() };
    const size_t blockSize = file.u32(bank + 8);
    if (bank + 16 + blockSize > data.size())
        return false;
    Reader block{ &data[bank + 16], blockSize };
    const uint32_t count = block.u32(0), table = block.u32(4);
    if (!block.ok || count > 256)
        return false;
    out.assign(count, SkeletonModel{});
    for (uint32_t k = 0; k < count; k++)
        if (!readModel(block, table + kSkeletonSize * k, table, out[k]))
            return false;
    return true;
}

} // namespace

bool loadSkeletonModels(const std::string& dataFolder, int level, const std::string& fileName,
                        std::vector<SkeletonModel>& out)
{
    const std::filesystem::path path = std::filesystem::path(dataFolder) / "levels" / std::to_string(level) / fileName;
    std::vector<uint8_t> data;
    if (!readDataFile(path.string(), data) || data.size() < 12 || std::memcmp(data.data(), "DRAM", 4) != 0)
        return false;
    Reader file{ data.data(), data.size() };
    // DRAM: size, number of pointers, block, pointers; then the G3DB bank
    const size_t bank = 12 + size_t(file.u32(4)) + 4 * size_t(file.u32(8));
    return file.ok && readBank(data, bank, out);
}

bool loadWeaponSounds(const std::string& dataFolder, const std::string& relativePath, std::array<int, 5>& out)
{
    out.fill(-1);
    std::vector<uint8_t> data;
    if (!readDataFile((std::filesystem::path(dataFolder) / relativePath).string(), data) || data.size() < 12 + 0x28
        || std::memcmp(data.data(), "WEAP", 4) != 0)
        return false;
    for (size_t k = 0; k < out.size(); k++) {
        const uint8_t slot = data[12 + 0xC + k];
        out[k] = slot == 0xFF ? -1 : int(slot);
    }
    return true;
}

bool loadWeaponActions(const std::string& dataFolder, const std::string& relativePath,
                       std::vector<std::array<int16_t, 10>>& out)
{
    std::vector<uint8_t> data;
    if (!readDataFile((std::filesystem::path(dataFolder) / relativePath).string(), data) || data.size() < 12
        || std::memcmp(data.data(), "WEAP", 4) != 0)
        return false;
    Reader file{ data.data(), data.size() };
    const size_t size = file.u32(4);
    if (12 + size > data.size())
        return false;
    out.clear();
    for (size_t at = 12 + 0x28; at + 20 <= 12 + size; at += 20) {
        std::array<int16_t, 10> row;
        for (int k = 0; k < 10; k++)
            row[size_t(k)] = int16_t(file.u16(at + 2 * size_t(k)));
        out.push_back(row);
    }
    return file.ok;
}

bool loadBankModels(const std::string& dataFolder, const std::string& relativePath, std::vector<SkeletonModel>& out)
{
    std::vector<uint8_t> data;
    if (!readDataFile((std::filesystem::path(dataFolder) / relativePath).string(), data))
        return false;
    // Go over the chunks before the bank: DRAM, HAND and WEAP are blocks with
    // pointers (FUN_0040410a): size, number of pointers, block, pointers
    Reader file{ data.data(), data.size() };
    size_t at = 0;
    while (at + 12 <= data.size()) {
        if (std::memcmp(&data[at], "G3DB", 4) == 0)
            return readBank(data, at, out);
        if (std::memcmp(&data[at], "DRAM", 4) != 0 && std::memcmp(&data[at], "HAND", 4) != 0
            && std::memcmp(&data[at], "WEAP", 4) != 0)
            return false;
        at += 12 + size_t(file.u32(at + 4)) + 4 * size_t(file.u32(at + 8));
    }
    return false;
}
