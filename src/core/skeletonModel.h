#pragma once

// Models with a skeleton (characters, monsters, objects) of the data of a
// level for a character: levels\<level>\CarnbyNN and AlineNN, a DRAM chunk
// and a G3DB bank (docs/FORMATOS.md). Read in their rest pose.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// A bone: it moves the vertices [firstVertex, endVertex)
struct ModelBone {
    float offset[3];  // from its parent
    float rest[3];    // place in the rest pose
    int parent;       // -1: none
    uint32_t firstVertex, endVertex;
};

// A vertex in the rest pose: centimetres, z up, the model looks along +y
struct ModelVertex {
    float x, y, z;
    float nx, ny, nz;  // normal
    float u, v;        // place on the texture (0..1)
};

// The triangles drawn with one texture (FUN_004896e0)
struct ModelMaterial {
    int texture;                   // -1: none
    uint32_t flags;                // 0x01 both sides; 0x02, 0x04 kinds of transparency
    std::vector<uint32_t> triangles;  // three vertex numbers each
};

// A key of an animation track: a value (position, angles in radians or
// scale) at a moment, in milliseconds. Between two keys the game goes in a
// straight line (angles by the short way)
struct AnimationKey {
    float value[3];
    float time;
};

// What an animation does to one bone (0x14 bytes in the bank; FUN_00427250,
// FUN_00413130, FUN_004132e8, FUN_00413528). A list without keys leaves the
// bone as in the rest pose
struct AnimationTrack {
    uint32_t bone;
    std::vector<AnimationKey> positions, rotations, scales;
};

// A group of tracks of an animation with its own clock, or a fixed pose
struct AnimationGroup {
    uint32_t length, flags;        // milliseconds; 0x01 loops
    std::vector<uint32_t> tracks;
};

// A group of tracks mixed from several sources with weights (aiming up,
// down...): every source has the same number of tracks, one per bone, and
// the weight of a slot that the game fills as it runs
struct AnimationMix {
    uint32_t length, flags;
    struct Source {
        std::vector<uint32_t> tracks;
        uint32_t weightSlot;
    };
    std::vector<Source> sources;
};

// How an animation passes to another one (0x10 bytes; FUN_00427030,
// FUN_004287d0, FUN_004282d0). Without windows it may pass at any moment;
// with them only while the current animation is inside one of them, and the
// new one starts at the matching moment of the window (from the foot that is
// ahead, say). Passing, the pose of that moment is blended into the new one
// for duration milliseconds while the character moves at speed
struct AnimationTransition {
    int target = -1;       // the animation it passes to
    float speed = 0;       // centimetres a frame, ahead, while blending (the game drops the fraction)
    uint32_t duration = 0; // milliseconds of blending (0: at once)
    struct Window {
        uint16_t from, to;             // moments of the current animation
        uint16_t targetFrom, targetTo; // where the new one starts, matching
    };
    std::vector<Window> windows;
};

// An animation (0x30 bytes in the bank): its length, its tracks and its
// groups (FUN_00427250: tracks, groups with their own clock, mixes, fixed).
// How it moves the character (FUN_004285a0): with flag 0x04 straight ahead
// at its speed; without it, as its first track (the root bone) goes
// Something that happens at a moment of an animation (8 bytes, list at
// +0x2C of the animation, number at +0x12; FUN_004270f0 gathers the ones
// passed each frame): code 1, a blow that hurts the hero if it is within
// value cm (FUN_00452b31); 2 a grip; 200, 999, 0x29A, 0x309 sounds and
// others not studied; 0x66 the hero's left hand 2 while it lasts
struct AnimationEvent {
    uint16_t time;   // milliseconds
    uint16_t code;
    uint16_t value;
    uint16_t more;
};

struct ModelAnimation {
    uint32_t length;               // milliseconds (multiples of 33: 30 frames a second)
    uint32_t flags;                // 0x01 loops, 0x02 backwards, 0x04 steady speed
    float speed;                   // centimetres a frame of 30 a second, ahead (+y)
    uint32_t rate;                 // playing speed: 0x1000 normal
    std::vector<uint32_t> tracks;  // numbers of the model's tracks
    std::vector<AnimationGroup> clocked, fixedPoses;
    std::vector<AnimationMix> mixes;
    std::vector<AnimationTransition> transitions;
    std::vector<AnimationEvent> events;
};

// Weights of the mixes when the game starts a model (FUN_004254d0): the
// first four slots at 0.25, the average of four poses (aiming straight ahead)
constexpr int kMixSlots = 16;

// A square texture: size x size colours 0xAABBGGRR, alpha 0 or 255
struct ModelTexture {
    int size = 0;
    std::vector<uint32_t> rgba;
};

struct SkeletonModel {
    std::vector<ModelBone> bones;
    std::vector<ModelVertex> vertices;
    std::vector<ModelMaterial> materials;
    std::vector<ModelTexture> textures;
    std::vector<AnimationTrack> tracks;
    std::vector<ModelAnimation> animations;
    // Animation identifiers: identifier = row x 10 + column, the value is the
    // number of an animation (-1 none). The actions of the weapons
    // (loadWeaponActions) ask for animations by identifier
    std::vector<std::array<int16_t, 10>> identifiers;
};

// The animation of a model with an identifier (row x 10 + column of its
// table). -1 if there is none
int animationWithIdentifier(const SkeletonModel& model, int identifier);

// Where a bone is in a pose: a 4x4 matrix, row after row, for points as rows
// (point x matrix)
struct BoneMatrix {
    float m[4][4];
};

// Animation flags
constexpr uint32_t kAnimationLoops = 0x01;
constexpr uint32_t kAnimationBackwards = 0x02;
constexpr uint32_t kAnimationSteady = 0x04;

// What advanceAnimation says of the clock
enum class AnimationClock { Running, Looped, Ended };

// Moves the clock of an animation (milliseconds; backwards ones go from their
// length to 0) on by some milliseconds of play, as FUN_00425199: looping ones
// start again, the others stop at their end
AnimationClock advanceAnimation(const ModelAnimation& animation, float& time, float elapsed);

// Where the root bone (bone 0) of an animation is at a moment: its place and
// its angles, from the animation's first track if it is of bone 0. False if
// it is not (FUN_004285a0 reads that track for the moves of the character)
bool rootMotion(const SkeletonModel& model, size_t animation, float time, float place[3], float angles[3]);

// The bones of a model in the pose of one of its animations at a moment
// (milliseconds), as the game puts the skeleton together (FUN_00426ab0):
// every bone turns about z, then y, then x, is scaled, moved to its place and
// carried by its parent. The tracks of bone 0 are left out (FUN_004279c0):
// they move the character, see rootMotion. An animation number out of range:
// the rest pose
void poseSkeleton(const SkeletonModel& model, size_t animation, float time, std::vector<BoneMatrix>& out);
// The same with the weights of the mixes (kMixSlots of them)
void poseSkeleton(const SkeletonModel& model, size_t animation, float time, const float weights[kMixSlots],
                  std::vector<BoneMatrix>& out);

// What an animation says of every bone at a moment, before the skeleton is
// put together: place, angles (radians) and scale; the rest pose where it says
// nothing (bone 0 left out, as in poseSkeleton)
struct BonePose {
    std::vector<std::array<float, 3>> place, angles, scale;
};
void sampleBonePose(const SkeletonModel& model, size_t animation, float time, BonePose& out);
void sampleBonePose(const SkeletonModel& model, size_t animation, float time, const float weights[kMixSlots],
                    BonePose& out);

// The skeleton of a bone pose (what poseSkeleton does after sampling)
void buildSkeleton(const SkeletonModel& model, const BonePose& pose, std::vector<BoneMatrix>& out);

// A pose between two: 0 the first, 1 the second, in a straight line (angles
// the short way round), as the game blends animations (FUN_004282d0)
void blendBonePoses(const BonePose& from, const BonePose& to, float amount, BonePose& out);

// The transition of an animation to another one, or nullptr if it has none
const AnimationTransition* findTransition(const SkeletonModel& model, size_t from, size_t to);

// The vertices of a model with its bones in a pose: every vertex goes with
// its bone from the bone's rest place
void skinVertices(const SkeletonModel& model, const std::vector<BoneMatrix>& bones, std::vector<ModelVertex>& out);

// The vertices of a part held by a bone (a hand, a weapon), in its rest pose:
// they are measured from the bone (the wrist)
void attachVertices(const SkeletonModel& part, const BoneMatrix& bone, std::vector<ModelVertex>& out);

// The wrist of a character: the bone at the end of an arm, which has no
// vertices (the hands are separate models: lefthand\ and weapon\). Left is
// towards -x. -1 if there is none
int wristBone(const SkeletonModel& model, bool left);

// Reads the models with a skeleton of levels\<level>\<fileName> (Carnby01,
// Aline01...). False if the file is missing or is not what is expected
bool loadSkeletonModels(const std::string& dataFolder, int level, const std::string& fileName,
                        std::vector<SkeletonModel>& out);

// The table of actions of a weapon (weapon\<character>\righthd, weaponN):
// in its WEAP chunk, after 0x28 bytes, rows of 10 animation identifiers, one
// row per action ("base") of the character's brain (FUN_00436236) and one
// column per variation (FUN_0043a286). False if the file has no WEAP chunk
// The sounds of a weapon: slots of its own sound bank (bank 1, its DSND
// chunk), bytes +0xC to +0x10 of its WEAP chunk (-1 none). The game plays
// +0xC as it fires (FUN_0046a0e2), +0xF when it would fire without bullets
// (the empty click, FUN_00469cd4), +0xD for the moment 1 of the hero's
// animations and +0x10 for 2 (if +0xE is -1) and 3 (FUN_0046cda4)
bool loadWeaponSounds(const std::string& dataFolder, const std::string& relativePath, std::array<int, 5>& out);

bool loadWeaponActions(const std::string& dataFolder, const std::string& relativePath,
                       std::vector<std::array<int16_t, 10>>& out);

// Reads the models with a skeleton of any file with a G3DB bank after other
// chunks (lefthand\carnby\hand1: HAND; weapon\carnby\weapon0: WEAP...),
// relative to the data folder. The bank is the first G3DB chunk
bool loadBankModels(const std::string& dataFolder, const std::string& relativePath, std::vector<SkeletonModel>& out);
