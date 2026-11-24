#pragma once

// The logic of a level for a character: the DRAM chunk of
// levels\<level>\CarnbyNN or AlineNN (docs/FORMATOS.md; FUN_0044a39d reads
// its header). For now its collision map, its trigger map with what its walls
// and zones do, where the character comes in, and its texts. Its scripts are
// places in the block.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "characterBrain.h"
#include "collisionMap.h"
#include "triggerMap.h"

// What crossing a wall of the trigger map does (+0x3AC, 0x10 bytes each;
// FUN_00466ee8): wall k goes with pass trigger k
struct PassTrigger {
    int camera = -1;          // only while this camera is shown (-1: any)
    uint32_t enterScript = 0; // crossing from the wall's front to its back (0: none)
    uint32_t leaveScript = 0; // the other way
};

// What a zone of the trigger map does (+0x3B0, 0x20 bytes each;
// FUN_00466f84, FUN_00467031, FUN_00449cb7): zone k goes with this k
struct ZoneTrigger {
    uint32_t flags = 0;        // 0x02: the action needs the character to face a way
    uint32_t enterScript = 0;  // going in (0: none)
    uint32_t leaveScript = 0;  // going out
    uint32_t actionScript = 0; // the action button pressed inside
    int faceAngle = 0;         // degrees, with flag 0x02 (not checked yet)
    int faceMargin = 0;
};

// A way into the level (+0x3B4, 0x24 bytes each; FUN_0044b91b)
struct LevelEntry {
    float place[3] = {};      // world
    float heading = 0;        // radians about z
    int fromLevel = -1;       // the level one comes from (-1: the start of the game)
    int camera = -1;          // the camera shown then
    bool forced = false;      // used whatever level one comes from
    uint32_t script = 0;      // run then (it may choose another camera)
};

// A place marked in the level (+0x3D8, 0x1C bytes each): scripts put the
// character there (function 72, FUN_00434a0e)
struct LevelMarker {
    float place[3] = {};   // world
    float angles[3] = {};  // radians; the one about z is the heading
};

// The lists of things of the level (FUN_0044a39d makes an object of each
// entry; not read yet), by what scripts ask for them with
enum LevelList {
    kListActors,    // +0x3B8 (0xD0 bytes each), number at +0x396: function 65
    kListObjects,   // +0x3BC (0xB4), +0x398: function 37
    kListMovers,    // +0x3C0 (0x40), +0x39A: function 166
    kListThings,    // +0x3C4 (0x1C), +0x39C: function 116
    kListOthers,    // +0x3C8 (0x10), +0x39E: function 129
    kLevelLists
};

// A thing of the level with a model of the level's bank (+0x3BC of the DRAM,
// 0xB4 bytes each; the game's object of 0x9C bytes, FUN_0046850c)
struct LevelObject {
    float place[3] = {};   // world
    float angles[3] = {};  // radians (x, y, z), turned as the character
    int model = -1;        // in the level's bank (with flag 0x04)
    int animation = 0;     // the one played while animating (+0x48 of the game's object)
    uint16_t flags = 0;    // 0x02 seen, 0x04 has a model, 0x08 animated, 0x10 shadow, 0x20 shadow seen, 0x80 more parts
    bool visible = false;  // flag 0x02 at first; scripts change it (functions 38, 39)
    bool shadowVisible = false;  // flag 0x20 at first (function 97)
    bool animating = false;      // flag 0x08 at first; function 40 starts an animation (not played yet)
};

// A particle system of the level put somewhere (+0x3C4 of the DRAM, 0x1C
// bytes each; the game's object of 0x50 bytes, FUN_0044f5b5): lightning in the
// sky, a light far away... Shown for some milliseconds by the scripts
// (function 117) and the scenes (order 20), or always (flag 0x01)
struct LevelThing {
    int system = 0;          // of the level's bank (+1)
    uint16_t flags = 0;      // +2
    float place[3] = {};     // world (+4)
    float angles[3] = {};    // radians (+0x10)
};

constexpr uint16_t kObjectSeen = 0x02, kObjectHasModel = 0x04, kObjectAnimated = 0x08, kObjectShadowSeen = 0x20;

// Where an actor is in its life (+0x2B2 of the game's actor): the game's
// numbers; 3, 4 and 5 are on the way to 1, 0 and 2
enum class ActorPresence : int {
    Away = 0,     // not in the level, not drawn
    Here = 1,     // in the level and drawn
    Done = 2,     // dead or finished (its class puts it there after its animation 5)
    Coming = 3,   // appearing to act (order 2)
    Leaving = 4,  // order 4
    Dying = 5,    // order 5
};

// An actor of the level: a monster or another character (+0x3B8 of the DRAM,
// 0xD0 bytes each; FUN_00454b20 makes one of 16 classes by its kind, and all
// read the record with FUN_00450c42)
struct LevelActor {
    int model = -1;         // in the level's bank
    int kind = 0;           // its class (+8)
    float place[3] = {};    // world
    float angles[3] = {};   // radians, turned as the things with a model
    ActorPresence presence = ActorPresence::Away;
    bool shown = false;     // drawn (+0x27C)
    int behaviour = 0;      // +0x268 (function 66; 2 with start state 2)
    bool behaviourFresh = true;  // the behaviour has just started (+0x270)
    int life = -1;          // life points (+0x252; -1: not given yet, from NpcCarac.csv)
    int walkVariant = -1;   // added to its walking animation, chosen once by its class (+0x34C; -1: not yet)
    // Where it goes to by itself (+0x274: the PHOTOSAURE backing away), and
    // for how long it has been going (milliseconds)
    float goal[3] = {};
    float goingFor = 0;
    CharacterMover body;    // its animation and where it is (core/actorBrain)
};

struct LevelLogic {
    std::vector<uint8_t> block;  // the DRAM block: the scripts are places in it
    bool hasCollision = false;
    CollisionMap collision;
    TriggerMap triggers;
    std::vector<PassTrigger> passTriggers;
    std::vector<ZoneTrigger> zoneTriggers;
    uint32_t levelScript = 0;    // run when entering the level, before the entry's (+0x14)
    uint32_t noActionScript = 0; // the action button pressed outside any zone with one (+0x1C)
    std::vector<LevelEntry> entries;
    std::vector<LevelMarker> markers;
    std::vector<LevelObject> objects;
    std::vector<LevelActor> actors;
    std::vector<LevelThing> things;
    // The level's flags (+0x10): 0x02 it rains (its system at +0x3F1),
    // 0x04 it rains from the start (function 105 changes it), 0x08 splashes
    // under the feet (the system at +0x3F3); FUN_0044a39d, FUN_0044b088
    uint32_t flags = 0;
    // The models' ambient colour (FUN_0044a39d -> FUN_00418696): +0x24 while
    // the level is lit (its state's bit 0x10), +0x28 in the dark (0..255)
    uint8_t ambientLit[3] = {}, ambientDark[3] = {};
    int rainSystem = -1;
    int splashSystem = -1;
    // The slots of the level's sound bank (-1: none) for the game's sounds
    // (+0x34 of the DRAM: 32 general ones, FUN_0044cac0) and for the 24
    // sounds of each kind of actor (+0x74, 0x30 bytes a kind, FUN_0044cb04)
    std::array<int16_t, 32> generalSounds{};
    std::array<std::array<int16_t, 24>, 16> actorSounds{};
    // The sounds of each kind of floor (+0x3DC, number at +0x3A8; 0x90 bytes,
    // 72 slots each): the moments of code 100 of the hero's animations play
    // the slot of their value (footsteps; FUN_00434409, FUN_0044cb6b)
    std::vector<std::array<int16_t, 72>> floorSounds;
    // Scripts by number (+0x3E0, 8 bytes each: number, script; +0x3AA how
    // many): scripts and the game start them by number (FUN_0044a1b7)
    std::vector<std::pair<int, uint32_t>> events;
    int listSizes[kLevelLists] = {};
    // Its texts (+0x374): the ones the English version shows; the Spanish
    // one shows others instead (docs/FORMATOS.md, "Textos e idiomas")
    std::vector<std::string> texts;
};

// Reads the logic of levels\<level>\<fileName>. False if the file is missing
// or is not a DRAM of the version the game takes (0x0131554C)
bool loadLevelLogic(const std::string& dataFolder, int level, const std::string& fileName, LevelLogic& out);

// The way in when coming from a level (-1: starting the game), as the game
// chooses it (FUN_0044b3e1): the forced one, else the one from that level;
// the first if none fits. nullptr if the level has none
const LevelEntry* chooseEntry(const LevelLogic& logic, int fromLevel);
