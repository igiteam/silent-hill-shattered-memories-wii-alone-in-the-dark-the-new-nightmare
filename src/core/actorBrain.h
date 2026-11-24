#pragma once

// The brains of the actors of the level (monsters and other characters).
// Every class of actor has its own (FUN_00454b20 makes them; entry +0x24 of
// their tables runs each frame); this is the one of the base class
// (FUN_00451853), with the reach and turning of each class from their
// constructors, for every class. What the behaviours do (+0x268; the scripts
// set them with function 66):
//
//   0       nothing (here it stands, playing its animation 1)
//   1       waits (animation 1) until the hero comes within its sight, then 4
//   2       faces the hero and attacks (animation 3); once the attack is over,
//           if the hero is out of reach, 4
//   3, 9    back to 1
//   4       faces the hero and goes towards it (animation 2); within reach, 2
//   5       goes to a place and starts a script (not done: it stands)
//   7, 8    hurt (animation 4): with no life left it dies, else 4
//
// The animations are identifiers of the model's table (setModelAnimation,
// FUN_0041806d): 1 standing, 2 walking, 3 attacking, 4 hurt, 5 dying.
//
// The PHOTOSAURE has its own (FUN_0045ce8f):
//
//   1       waits (1) until it sees the hero, roars (16), then 4
//   2       goes for the hero (2; 3 once it has fled); in the flashlight's
//           beam it flees (0xD to 0x10); ahead of it, at 220 to 250 cm it
//           leaps (0x12), at 80 to 100 bites (0x13), nearer it backs away
//           (0x11)
//   4       goes for the hero; then 2 (the game looks for it by the level's
//           waypoints: not done)
//   7, 8    hurt (8), then 2; with no life left it dies
//   0xC     as 2, running (3, or 10 within 250 cm)
//   0xD..10 flees (17, 19, 18 or 20), then 0x15 or 0x16
//   0x11    backs away to a place 2 m behind it (the game: the nearest
//           waypoint), then 2 or 0xC
//   0x12    leaps at the hero (4; its blow reaches 300 cm), then 0x15/0x16
//   0x13    bites (6; 100 cm), then 0x15/0x16
//   0x15/16 recovers (11 or 10), then 2 or 0xC
//
// The HYBRIDE's own brain (FUN_0043f8cf: holding the hero, going round
// it, several attacks...) is not done yet

#include <vector>

#include "collisionMap.h"
#include "levelLogic.h"
#include "npcTable.h"
#include "skeletonModel.h"

// What makes the classes differ in the base brain (their constructors)
struct ActorClass {
    float sight = 400;       // cm: within it, the hero is seen (+0x258)
    float reach = 120;       // cm: within it, it attacks (+0x25C)
    float turning = 0x400;   // how fast it faces the hero, of 4096 per frame of 30 a second (+0x54)
    bool randomWalk = false; // walks with 2 or 27, chosen at random once (HYBRIDE, +0x34C)
    // The identifiers of its animations: the base class's 1 to 5; the classes
    // with brains of their own use others (their attacks carry the blow, the
    // moment of code 1)
    int standing = 1, walking = 2, attacking = 3, hurt = 4, dying = 5;
};

// The class of a kind of actor (+8 of its record: NpcCarac.csv names them)
ActorClass actorClass(int kind);

// One frame of an actor that is here or dying: its brain chooses what to do
// and its animation, and its body moves (against the collision map, nullptr:
// anywhere). The hero is where its feet are (heroAlive: it is not dead);
// npc: its kind's characteristics (nullptr: none). The pose to show is left
// in actor.body.pose. Returns the damage its blows did to the hero this
// frame (the moments of code 1 of its animations, FUN_00452b31), 0 if none;
// the sounds it makes (numbers of its kind's sounds) are added to sounds
// inLight: the flashlight's beam is on it (the PHOTOSAURE flees)
int updateActor(LevelActor& actor, const SkeletonModel& model, const float hero[3], bool heroAlive,
                float milliseconds, const CollisionMap* collision, const NpcCharacteristics* npc,
                std::vector<int>* sounds = nullptr, bool inLight = false);

// The sounds of its kind an actor makes at a moment of code 201 of its
// animation, by its value (FUN_00452b31; several pick one at random, some
// only now and then): numbers 0..23 of the kind's sounds
void actorEventSounds(int value, std::vector<int>& out);

// The actor the hero aims at (the weapons that lock on by themselves, FUN_00438920
// looks for one): the nearest one here within range (cm) and within the cone
// (radians either side of where the hero looks). -1: none
int aimedActor(const std::vector<LevelActor>& actors, const CharacterMover& hero, float range, float cone);

// A shot hits an actor (FUN_00452096): it loses the damage from its life and
// is hurt (behaviour 7); its brain kills it once the hurt is over if it has
// no life left. npc: its kind's characteristics, for its life if not given yet
void hurtActor(LevelActor& actor, int damage, const NpcCharacteristics* npc);
