#pragma once

// The character the player leads (GE_Hero): its controls (FUN_00438920), its
// brain (FUN_00436236), which turns the buttons into an action and so into an
// animation, and how it moves on the floor (FUN_004285a0, FUN_00434802).
// For now the brain knows standing, walking, running, walking back, turning
// and the half-turn; it bumps into the walls of the level's collision map
// and stands on its floors (docs/ALONE4_EXE.md).

#include <array>
#include <cstdint>
#include <vector>

#include "collisionMap.h"
#include "skeletonModel.h"

// The character against the collision map: a sphere of this radius with its
// centre this high over the feet (FUN_00433bf5)
constexpr float kCharacterRadius = 20.0f;
constexpr float kCharacterCentreHeight = 60.0f;

// The buttons of the brain (FUN_00436130 makes them from the controls)
constexpr uint32_t kButtonForward = 0x01;
constexpr uint32_t kButtonBack = 0x02;
constexpr uint32_t kButtonRun = 0x04;
constexpr uint32_t kButtonTurnLeft = 0x08;
constexpr uint32_t kButtonTurnRight = 0x10;
constexpr uint32_t kButtonHalfTurn = 0x20;
constexpr uint32_t kButtonAim = 0x40;     // the weapon held up
constexpr uint32_t kButtonAction = 0x100; // use, or shoot while aiming

// The keys held this frame
struct CharacterKeys {
    bool forward = false, back = false, left = false, right = false, run = false;
    bool aim = false, action = false;
};

// What the controls remember between frames: back pressed twice in a short
// time is a half-turn, and back alone only walks back once held a moment
struct CharacterControls {
    bool backBefore = false;  // back was held the frame before
    bool waiting = false;     // after a press of back, for its release
    bool tapped = false;      // back was pressed and let go quickly
    bool backBlocked = false; // back does not walk back yet
    float sincePress = 0;     // milliseconds since that press
};

// The buttons of a frame from the keys held, milliseconds since the last one
uint32_t characterButtons(CharacterControls& controls, const CharacterKeys& keys, float milliseconds);

// The states of the brain, with the game's numbers (the ones used for now)
enum class CharacterState : uint32_t {
    Idle = 0,
    Walk = 1,
    ShootWalking = 2,
    WalkAiming = 3,
    WalkBack = 4,
    ShootWalkingBack = 5,
    WalkBackAiming = 6,
    Turn = 7,
    Run = 8,
    Aim = 9,
    Shoot = 10,
    Dead = 0x10,
    Hurt = 0x11,
    HalfTurn = 0x13,
    HalfTurnAiming = 0x14,
};

// A name for a state, for the viewer's title
const char* characterStateName(CharacterState state);

// The character: its brain, its animation and where it stands in the world
// (centimetres, z up; the feet). Heading in radians about z: at 0 it looks
// along +y, more is to its left
struct CharacterMover {
    CharacterState state = CharacterState::Idle;
    CharacterState nextState = CharacterState::Idle;  // from the next frame on (FUN_00435ffc)
    int animation = -1;
    float time = 0;               // of the animation, milliseconds
    float rootPlace[3] = {}, rootAngles[3] = {};  // root bone the frame before
    float x = 0, y = 0, z = 0, heading = 0;
    BonePose pose;  // the pose to show this frame
    // Passing from one animation to another (FUN_004282d0): the pose shown
    // then blends into the new one taken at blendStart, for blendDuration
    // milliseconds, the character moving ahead at blendSpeed
    bool blending = false;
    BonePose blendFrom, blendTo;
    int blendTarget = -1;
    float blendStart = 0, blendTime = 0, blendDuration = 0, blendSpeed = 0;
    // A weapon is held (FUN_004696e0): the aim button aims only with one
    bool armed = false;
    // A shot left this frame (the weapon fires, its entry +0x20, as the
    // action button is pressed while aiming)
    bool fired = false;
    // The moments of the animation passed this frame (FUN_004270f0)
    std::vector<AnimationEvent> events;
    // Hurt and dead (states 0x11 and 0x10, FUN_00435281, FUN_00435338): the
    // animation of the blow (by identifier), and whether the fall is over
    // (then it lies, 0x8D)
    int hurtAnimation = 14;
    bool lying = false;
    float sinceDeath = 0;  // milliseconds lying
    // The kind of floor under its feet, for the footsteps (-1: none)
    int floorKind = -1;
};

// A blow reaches the character (FUN_00435281): hurt with that animation (an
// identifier), or dead if its life is over (FUN_00435338)
void hitCharacter(CharacterMover& character, int animation, bool dead);

// Whether a state holds the weapon up (aiming, shooting)
bool aimingState(CharacterState state);

// One frame of the character: the brain chooses the action with the buttons,
// its animation comes from the actions of the weapon in the right hand (rows
// of 10 identifiers; column = variation, 1 with the flashlight on) and plays
// on (passing from one to another through their transitions), the pose to
// show is left in pose, and the character moves and turns. With a collision
// map it bumps into its walls and stands on its floors (nullptr: anywhere)
void moveCharacter(CharacterMover& character, const SkeletonModel& model,
                   const std::vector<std::array<int16_t, 10>>& actions, int variation, uint32_t buttons,
                   float milliseconds, const CollisionMap* collision);

// One frame of a body (the character or an actor) playing an animation
// (FUN_004287d0): the same one plays on, another one starts at once or
// through its transition, the pose to show is left in pose, and the body
// moves as the animation says (FUN_004285a0) and turns by it and by turnBy
// (radians, more is to the left), against the walls of the collision map
// (nullptr: anywhere). Says whether the animation looped or ended this frame
AnimationClock moveBody(CharacterMover& body, const SkeletonModel& model, int animation, float milliseconds,
                        float turnBy, const CollisionMap* collision);

// Puts the character on the floor of the collision map under it, if there is
// one (FUN_004032c2)
void standOnFloor(CharacterMover& character, const CollisionMap& collision);
