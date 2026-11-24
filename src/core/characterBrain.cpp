#include "characterBrain.h"

#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979f;

// Back pressed again within this time: a half-turn (FUN_00438920)
constexpr float kDoubleTapMilliseconds = 300.0f;

// Turning with the buttons: 6 degrees a frame of 33 milliseconds (FUN_00434802)
constexpr float kTurnPerMillisecond = 6.0f * 2.0f * kPi / 360.0f / 33.0f;

// Speeds are in centimetres a frame: frames in a millisecond (DAT_004bd6c8)
constexpr float kFramesPerMillisecond = 0.030003f;

// The action ("base") of each state: the row of the actions of the weapon
int stateAction(CharacterState state)
{
    switch (state) {
    case CharacterState::Walk: return 1;
    case CharacterState::Run: return 2;
    case CharacterState::Turn: return 6;
    case CharacterState::WalkBack: return 9;
    case CharacterState::HalfTurn: return 12;
    case CharacterState::Aim: return 4;
    case CharacterState::Shoot: return 5;
    case CharacterState::ShootWalking:
    case CharacterState::WalkAiming: return 7;
    case CharacterState::ShootWalkingBack:
    case CharacterState::WalkBackAiming: return 10;
    case CharacterState::HalfTurnAiming: return 13;
    default: return 0;
    }
}

// The animation of an action with a variation (FUN_0043a286, FUN_00469ff3),
// or with variation 0 if that one has none. -1 if there is none
int actionAnimation(const SkeletonModel& model, const std::vector<std::array<int16_t, 10>>& actions, int action,
                    int variation)
{
    if (action < 0 || size_t(action) >= actions.size())
        return -1;
    const std::array<int16_t, 10>& row = actions[size_t(action)];
    int animation = variation >= 0 && variation < 10 ? animationWithIdentifier(model, row[size_t(variation)]) : -1;
    if (animation < 0)
        animation = animationWithIdentifier(model, row[0]);
    return animation;
}

// The next state from the buttons (FUN_00436236): armed, a weapon is held
CharacterState nextState(CharacterState state, uint32_t buttons, AnimationClock clock, bool armed)
{
    const bool forward = buttons & kButtonForward, back = buttons & kButtonBack, run = buttons & kButtonRun;
    const bool turn = buttons & (kButtonTurnLeft | kButtonTurnRight);
    const bool aim = (buttons & kButtonAim) && armed, action = buttons & kButtonAction;
    switch (state) {
    case CharacterState::Idle:
        if (aim)
            return CharacterState::Aim;
        if (forward)
            return run ? CharacterState::Run : CharacterState::Walk;
        if (buttons & kButtonHalfTurn)
            return CharacterState::HalfTurn;
        if (back)
            return CharacterState::WalkBack;
        if (turn)
            return CharacterState::Turn;
        return state;
    case CharacterState::Walk:
        if (!forward)
            return CharacterState::Idle;
        if (run)
            return CharacterState::Run;
        return aim ? CharacterState::WalkAiming : state;
    case CharacterState::Run:
        if (!forward)
            return CharacterState::Idle;
        return run ? state : CharacterState::Walk;
    case CharacterState::WalkBack:
        if (!back)
            return CharacterState::Idle;
        return aim ? CharacterState::WalkBackAiming : state;
    case CharacterState::Aim:
        // Held up: let go, it lowers; it walks, turns round or shoots
        if (!aim)
            return CharacterState::Idle;
        if (forward)
            return CharacterState::WalkAiming;
        if (buttons & kButtonHalfTurn)
            return CharacterState::HalfTurnAiming;
        if (back)
            return CharacterState::WalkBackAiming;
        return action ? CharacterState::Shoot : state;
    case CharacterState::Shoot:
        // The shot plays to its end, then back to aiming (weapons that fire
        // on and on, 2, 6 and 8, are not done)
        return clock == AnimationClock::Running ? state : CharacterState::Aim;
    case CharacterState::ShootWalking:
    case CharacterState::WalkAiming:
        if (!forward)
            return aim ? CharacterState::Aim : CharacterState::Idle;
        if (!aim)
            return CharacterState::Walk;
        return action ? CharacterState::ShootWalking : state;
    case CharacterState::ShootWalkingBack:
    case CharacterState::WalkBackAiming:
        if (!back)
            return aim ? CharacterState::Aim : CharacterState::Idle;
        if (!aim)
            return CharacterState::WalkBack;
        return action ? CharacterState::ShootWalkingBack : state;
    case CharacterState::HalfTurnAiming:
        if (clock == AnimationClock::Running)
            return state;
        return aim ? CharacterState::Aim : CharacterState::Idle;
    case CharacterState::Turn:
        if (forward)
            return CharacterState::Walk;
        return turn ? state : CharacterState::Idle;
    case CharacterState::HalfTurn:
        if (clock == AnimationClock::Running)
            return state;
        return aim ? CharacterState::Aim : CharacterState::Idle;
    case CharacterState::Hurt:
        // The blow plays to its end, then it stands
        return clock == AnimationClock::Running ? state : CharacterState::Idle;
    case CharacterState::Dead:
        return state;
    }
    return CharacterState::Idle;
}

// Where an animation starts: its beginning, or its end if it plays backwards
float startOf(const SkeletonModel& model, int animation)
{
    if (animation < 0 || !(model.animations[size_t(animation)].flags & kAnimationBackwards))
        return 0;
    return float(model.animations[size_t(animation)].length);
}

// Puts an animation on at a moment, its root bone from nothing
void startAnimation(CharacterMover& c, int animation, float time)
{
    c.animation = animation;
    c.time = time;
    for (int i = 0; i < 3; i++)
        c.rootPlace[i] = c.rootAngles[i] = 0;
}

// Starts blending the pose shown into a new animation at a moment
// (FUN_004287d0 with a transition): the new pose is taken at that moment and
// stays still while blending; the speed drops its fraction, as the game's
void beginBlend(CharacterMover& c, const SkeletonModel& model, const AnimationTransition& transition, float start)
{
    c.blendFrom = c.pose;
    sampleBonePose(model, size_t(transition.target), start, c.blendTo);
    c.blendTarget = transition.target;
    c.blendStart = start;
    c.blendTime = 0;
    c.blendDuration = float(transition.duration);
    c.blendSpeed = std::trunc(transition.speed);
    c.blending = true;
}

// Plays an animation on (FUN_00425199) and gathers the moments of it passed
// (FUN_004270f0): from the time before to the time after, round the end if
// it looped; backwards ones the other way
AnimationClock advanceGathering(CharacterMover& c, const ModelAnimation& a, float elapsed)
{
    const float before = c.time;
    const AnimationClock clock = advanceAnimation(a, c.time, elapsed);
    const float after = c.time, length = float(a.length);
    const bool backwards = (a.flags & kAnimationBackwards) != 0;
    for (const AnimationEvent& e : a.events) {
        const float t = float(e.time);
        bool passed;
        if (!backwards)
            passed = clock == AnimationClock::Looped ? (t >= before || t < after)
                                                     : (t >= before && (t < after || (clock == AnimationClock::Ended && t <= length)));
        else
            passed = clock == AnimationClock::Looped ? (t < before || t >= after) : (t < before && t >= after);
        if (passed)
            c.events.push_back(e);
    }
    return clock;
}

} // namespace

uint32_t characterButtons(CharacterControls& c, const CharacterKeys& keys, float milliseconds)
{
    uint32_t buttons = 0;
    // Back: the first press waits for its release; pressed again soon after
    // a quick release, a half-turn; held a moment, it walks back
    c.sincePress += milliseconds;
    const bool pressed = keys.back && !c.backBefore, released = !keys.back && c.backBefore;
    c.backBefore = keys.back;
    if (!c.waiting) {
        if (pressed) {
            if (!c.tapped) {
                c.backBlocked = true;
                c.waiting = true;
                c.sincePress = 0;
            } else {
                if (c.sincePress < kDoubleTapMilliseconds)
                    buttons |= kButtonHalfTurn;
                else {
                    c.waiting = true;
                    c.sincePress = 0;
                }
                c.tapped = false;
            }
        }
    } else {
        if (c.sincePress > kDoubleTapMilliseconds)
            c.backBlocked = false;
        if (released) {
            if (c.sincePress < kDoubleTapMilliseconds)
                c.tapped = true;
            c.waiting = false;
        }
    }
    if (keys.forward)
        buttons |= kButtonForward;
    if (keys.back && !c.backBlocked)
        buttons |= kButtonBack;
    if (keys.run)
        buttons |= kButtonRun;
    if (keys.left)
        buttons |= kButtonTurnLeft;
    if (keys.right)
        buttons |= kButtonTurnRight;
    if (keys.aim)
        buttons |= kButtonAim;
    if (keys.action)
        buttons |= kButtonAction;
    return buttons;
}

const char* characterStateName(CharacterState state)
{
    switch (state) {
    case CharacterState::Idle: return "idle";
    case CharacterState::Walk: return "walk";
    case CharacterState::Run: return "run";
    case CharacterState::WalkBack: return "walk back";
    case CharacterState::Turn: return "turn";
    case CharacterState::HalfTurn: return "half-turn";
    case CharacterState::Aim: return "aim";
    case CharacterState::Shoot: return "shoot";
    case CharacterState::ShootWalking: return "shoot walking";
    case CharacterState::WalkAiming: return "walk aiming";
    case CharacterState::ShootWalkingBack: return "shoot walking back";
    case CharacterState::WalkBackAiming: return "walk back aiming";
    case CharacterState::HalfTurnAiming: return "half-turn aiming";
    case CharacterState::Hurt: return "hurt";
    case CharacterState::Dead: return "dead";
    }
    return "?";
}

void standOnFloor(CharacterMover& c, const CollisionMap& collision)
{
    const float centre[3] = { c.x, c.y, c.z + kCharacterCentreHeight };
    float floorZ;
    int kind = -1;
    if (floorBelow(collision, centre, floorZ, &kind)) {
        c.z = floorZ;
        c.floorKind = kind;
    }
}

AnimationClock moveBody(CharacterMover& c, const SkeletonModel& model, int wanted, float milliseconds, float turnBy,
                        const CollisionMap* collision)
{
    // The animation (FUN_004287d0): the same one plays on; another one starts
    // at once, or through its transition (blending, maybe waiting for a window)
    c.events.clear();
    AnimationClock clock = AnimationClock::Running;
    float elapsed = 0;     // of the animation playing, for its moves
    bool plays = false;    // c.animation plays on this frame
    float blendAhead = 0;  // moves while blending
    if (c.blending) {
        // Blending (FUN_004282d0); what the brain wants waits until it ends
        c.blendTime += milliseconds;
        blendAhead = milliseconds * c.blendSpeed * kFramesPerMillisecond;
        if (c.blendTime < c.blendDuration)
            blendBonePoses(c.blendFrom, c.blendTo, c.blendTime / c.blendDuration, c.pose);
        else {
            // Ended: the new animation plays on from where it started, for the
            // time left over
            c.blending = false;
            startAnimation(c, c.blendTarget, c.blendStart);
            if (c.animation >= 0) {
                const ModelAnimation& a = model.animations[size_t(c.animation)];
                elapsed = (c.blendTime - c.blendDuration) * float(a.rate) / 4096.0f;
                clock = advanceGathering(c, a, elapsed);
                plays = true;
            }
        }
    } else if (wanted != c.animation) {
        const AnimationTransition* transition =
            c.animation >= 0 && wanted >= 0 && !c.pose.place.empty()
                ? findTransition(model, size_t(c.animation), size_t(wanted))
                : nullptr;
        if (!transition)
            startAnimation(c, wanted, startOf(model, wanted));
        else if (!transition->windows.empty()) {
            // Only inside a window, starting the new one at the matching moment
            const AnimationTransition::Window* window = nullptr;
            for (const AnimationTransition::Window& w : transition->windows)
                if (float(w.from) <= c.time && c.time <= float(w.to)) {
                    window = &w;
                    break;
                }
            if (window) {
                float start = window->targetFrom;
                if (window->to > window->from)
                    start += (c.time - float(window->from)) * float(window->targetTo - window->targetFrom)
                             / float(window->to - window->from);
                beginBlend(c, model, *transition, start);
            } else if (c.animation >= 0) {
                // Not yet: the current one plays on until it reaches a window
                const ModelAnimation& a = model.animations[size_t(c.animation)];
                elapsed = milliseconds * float(a.rate) / 4096.0f;
                clock = advanceGathering(c, a, elapsed);
                plays = true;
            }
        } else if (transition->duration > 0)
            beginBlend(c, model, *transition, startOf(model, wanted));
        else
            startAnimation(c, wanted, startOf(model, wanted));
    } else if (c.animation >= 0) {
        const ModelAnimation& a = model.animations[size_t(c.animation)];
        elapsed = milliseconds * float(a.rate) / 4096.0f;
        clock = advanceGathering(c, a, elapsed);
        plays = true;
    }
    if (c.animation < 0 && !c.blending)
        clock = AnimationClock::Ended;  // nothing to wait for
    if (!c.blending && c.animation >= 0)
        sampleBonePose(model, size_t(c.animation), c.time, c.pose);

    // How far the animation moves the character (FUN_004285a0): straight
    // ahead at its speed, or as its root bone goes since the frame before;
    // while blending, ahead at the transition's speed
    float move[3] = { 0, blendAhead, 0 }, turn = 0;
    if (plays) {
        const ModelAnimation& a = model.animations[size_t(c.animation)];
        float place[3], angles[3];
        if (a.flags & kAnimationSteady)
            move[1] = elapsed * a.speed * kFramesPerMillisecond;
        else if (rootMotion(model, size_t(c.animation), c.time, place, angles)) {
            if (clock == AnimationClock::Looped)
                for (int i = 0; i < 3; i++)
                    c.rootPlace[i] = c.rootAngles[i] = 0;
            for (int i = 0; i < 3; i++) {
                move[i] = place[i] - c.rootPlace[i];
                c.rootPlace[i] = place[i];
            }
            turn = angles[2] - c.rootAngles[2];
            for (int i = 0; i < 3; i++)
                c.rootAngles[i] = angles[i];
        }
    }

    // The turn asked for, then the move along the heading (FUN_00434802)
    c.heading += turn + turnBy;
    c.heading = std::remainder(c.heading, 2 * kPi);
    const float cs = std::cos(c.heading), sn = std::sin(c.heading);
    const float dx = cs * move[0] - sn * move[1], dy = sn * move[0] + cs * move[1];
    if (collision) {
        // Through the walls of the map, then onto its floor (FUN_00434d31)
        const float from[3] = { c.x, c.y, c.z + kCharacterCentreHeight };
        const float to[3] = { c.x + dx, c.y + dy, from[2] };
        float end[3];
        moveSphere(*collision, from, to, kCharacterRadius, end);
        c.x = end[0];
        c.y = end[1];
        standOnFloor(c, *collision);
    } else {
        c.x += dx;
        c.y += dy;
    }
    return clock;
}

void moveCharacter(CharacterMover& c, const SkeletonModel& model, const std::vector<std::array<int16_t, 10>>& actions,
                   int variation, uint32_t buttons, float milliseconds, const CollisionMap* collision)
{
    // The state chosen the frame before starts now, with its animation; a
    // shot leaves as shooting starts
    const CharacterState before = c.state;
    c.state = c.nextState;
    c.fired = c.state != before
              && (c.state == CharacterState::Shoot || c.state == CharacterState::ShootWalking
                  || c.state == CharacterState::ShootWalkingBack);
    int wanted = actionAnimation(model, actions, stateAction(c.state), variation);
    const bool helpless = c.state == CharacterState::Hurt || c.state == CharacterState::Dead;
    if (c.state == CharacterState::Hurt)
        wanted = animationWithIdentifier(model, c.hurtAnimation);
    if (c.state == CharacterState::Dead)
        wanted = animationWithIdentifier(model, c.lying ? 0x8D : 0x8C);
    // Turning with the buttons
    float turnBy = 0;
    if (!helpless && (buttons & kButtonTurnLeft))
        turnBy += kTurnPerMillisecond * milliseconds;
    if (!helpless && (buttons & kButtonTurnRight))
        turnBy -= kTurnPerMillisecond * milliseconds;
    const AnimationClock clock = moveBody(c, model, wanted, milliseconds, turnBy, collision);
    if (c.state == CharacterState::Dead) {
        if (clock != AnimationClock::Running)
            c.lying = true;  // fallen: it lies (0x8D)
        if (c.lying)
            c.sinceDeath += milliseconds;
    }
    c.nextState = nextState(c.state, buttons, clock, c.armed);
}

void hitCharacter(CharacterMover& c, int animation, bool dead)
{
    if (c.state == CharacterState::Dead)
        return;
    c.hurtAnimation = animation;
    c.state = c.nextState = dead ? CharacterState::Dead : CharacterState::Hurt;
    c.lying = false;
    c.sinceDeath = 0;
    c.animation = -1;  // the blow starts at once
    c.blending = false;
}

bool aimingState(CharacterState state)
{
    switch (state) {
    case CharacterState::Aim:
    case CharacterState::Shoot:
    case CharacterState::ShootWalking:
    case CharacterState::WalkAiming:
    case CharacterState::ShootWalkingBack:
    case CharacterState::WalkBackAiming:
    case CharacterState::HalfTurnAiming:
        return true;
    default:
        return false;
    }
}
