#include "actorBrain.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kFramesPerMillisecond = 0.030003f;  // frames of 30 a second (DAT_004bd6c8)

// The turn that faces a point (FUN_0042f4db): part of the way each frame, as
// much as the class turns (of 4096) per frame of 30 a second
float turnTowards(const CharacterMover& body, const float point[3], float turning, float milliseconds)
{
    const float dx = point[0] - body.x, dy = point[1] - body.y;
    if (dx == 0 && dy == 0)
        return 0;
    const float wanted = std::atan2(-dx, dy);  // looking along +y at 0, more to the left
    const float left = std::remainder(wanted - body.heading, 2 * kPi);
    const float perFrame = std::fmin(turning / 4096.0f, 1.0f);
    const float part = 1.0f - std::pow(1.0f - perFrame, milliseconds * kFramesPerMillisecond);
    return left * part;
}

// How far the hero is (FUN_0042f1f4 with its last argument 1: in 3D)
float distanceTo(const CharacterMover& body, const float point[3])
{
    const float dx = point[0] - body.x, dy = point[1] - body.y, dz = point[2] - body.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Bodies do not go into each other: the actor and the hero are cylinders of
// radius 20 (FUN_00450c42 gives the actor its own, like the hero's), so the
// actor is pushed back to 40 cm from the hero if it came nearer
void keepApart(CharacterMover& body, const float hero[3])
{
    constexpr float kApart = 2 * kCharacterRadius;
    const float dx = body.x - hero[0], dy = body.y - hero[1];
    const float d = std::sqrt(dx * dx + dy * dy);
    if (d >= kApart || std::fabs(body.z - hero[2]) > 100)
        return;
    if (d < 0.01f) {
        body.y = hero[1] - kApart;
        return;
    }
    body.x = hero[0] + dx * kApart / d;
    body.y = hero[1] + dy * kApart / d;
}

// Sets the behaviour from the next frame on, as just started (FUN_00452586)
void behave(LevelActor& actor, int behaviour)
{
    actor.behaviour = behaviour;
    actor.behaviourFresh = true;
}

} // namespace

ActorClass actorClass(int kind)
{
    ActorClass c;
    switch (kind) {
    case 0:  // PHOTOSAURE (FUN_0045ccd0): bites with 6 (behaviour 0x13 of its brain; blow at 100 cm)
        c.sight = 5000;
        c.reach = 100;
        c.attacking = 6;
        c.hurt = 8;
        break;
    case 10:  // HYBRIDE_2 and HYBRIDE (FUN_0043f710)
    case 15:
        c.sight = 2000;
        c.turning = 0x100;
        c.reach = 70;  // +0x344: within it, behaviour 0x1E attacks with 10 (blow at 80 cm)
        c.attacking = 10;
        c.randomWalk = true;
        break;
    default:
        break;
    }
    return c;
}

// Whether the hero is ahead of the actor (FUN_0045244e: within about 20
// degrees of where it looks)
bool heroAhead(const CharacterMover& body, const float hero[3])
{
    const float dx = hero[0] - body.x, dy = hero[1] - body.y;
    const float d = std::sqrt(dx * dx + dy * dy);
    if (d < 0.01f)
        return true;
    const float fx = -std::sin(body.heading), fy = std::cos(body.heading);
    return (fx * dx + fy * dy) / d * 4096.0f > float(0xEFF);
}

void actorEventSounds(int value, std::vector<int>& out)
{
    const int r = std::rand();
    if (value >= 0 && value <= 0x17)
        out.push_back(value);
    else if (value == 100)
        out.push_back(r % 6);
    else if (value == 0x65)
        out.push_back(r & 1);
    else if (value == 0x66)
        out.push_back((r & 1) + 2);
    else if (value == 0x67) {
        out.push_back((r & 1) + 4);
        out.push_back(std::rand() & 3);  // the game goes on into 0x68
    } else if (value == 0x68)
        out.push_back(r & 3);
    else if (value == 200)
        out.push_back(r % 5 + 0x13);
    else if (value == 0xC9)
        out.push_back((r & 1) + 0x13);
    else if (value >= 0xCA && value <= 0xCC) {
        if (r % 6 == 0)
            out.push_back(value - 0xCA + 0x15);
    } else if (value == 0xCD)
        out.push_back(r % 3 + 0x13);  // the game plays it three times in four
    else if (value == 0xCE)
        out.push_back(r % 3 + 0x15);
    else if (value == 300) {
        if ((r & 4) == 0)
            out.push_back((std::rand() & 1) + 0x11);
    } else if (value == 0x12D) {
        if ((r & 4) == 0)
            out.push_back(0x11);
    }
}

namespace {

// The PHOTOSAURE's distances (its constructor, +0x344 to +0x34A): it leaps
// from 220 to 250 cm, bites from 80 to 100, and backs away if nearer
constexpr float kLeapFar = 250, kLeapNear = 220, kBiteFar = 100, kBiteNear = 80;
constexpr float kBackAway = 200;       // cm behind it
constexpr float kGoalReached = 40;     // cm
constexpr float kGoalGiveUp = 2500;    // milliseconds (ours: it has no waypoints)

// After an attack or a flight, the PHOTOSAURE recovers (0x15 or 0x16, at random)
int photosaureRecovery()
{
    return (std::rand() & 1) + 0x15;
}

// Where it flees to (FUN_0045e006): 0 back, 1 or 2 to a side (the game
// checks that 120 cm that way is free); the behaviours 0xD + that
int photosaureFlight()
{
    const int r = std::rand() & 3;
    return 0xD + (r == 3 ? 0 : r);
}

// After recovering, it goes for the hero again: walking (2) or, once it
// has fled, running (0xC)
int photosaureChase(const LevelActor& actor)
{
    return actor.walkVariant == 0 ? 2 : 0xC;
}

// The PHOTOSAURE's brain before moving (FUN_0045ce8f): its animation and
// how much it turns this frame
void photosaureChooses(LevelActor& actor, const ActorClass& c, const float hero[3], bool heroAlive, bool inLight,
                       float milliseconds, int& animation, float& turnBy)
{
    CharacterMover& body = actor.body;
    const bool fresh = actor.behaviourFresh;
    actor.behaviourFresh = false;
    const float distance = distanceTo(body, hero);
    const int walking = actor.walkVariant + 2;  // 2 walks, 3 runs (+0x34C)
    animation = c.standing;
    switch (actor.behaviour) {
    case 1:
        if (fresh) {
            if (distance > c.sight)
                actor.behaviourFresh = true;  // still waiting
        } else
            animation = 0x10;  // roars; when it ends, 4
        break;
    case 2:
    case 0xC:
        animation = actor.behaviour == 2 || actor.walkVariant == 0 ? walking : distance < kLeapFar ? 10 : 3;
        turnBy = turnTowards(body, hero, c.turning, milliseconds);
        if (inLight) {
            actor.walkVariant = 1;
            behave(actor, photosaureFlight());
        } else if (!heroAlive || distance > c.sight)
            behave(actor, heroAlive ? 4 : 1);
        else if (heroAhead(body, hero)) {
            if (distance < kLeapFar && distance > kLeapNear)
                behave(actor, 0x12);
            else if (distance < kBiteFar && distance > kBiteNear)
                behave(actor, 0x13);
            else if (distance < kBiteFar) {
                // Backs away: to a place 2 m behind it
                actor.goal[0] = body.x + std::sin(body.heading) * kBackAway;
                actor.goal[1] = body.y - std::cos(body.heading) * kBackAway;
                actor.goal[2] = body.z;
                actor.goingFor = 0;
                behave(actor, 0x11);
            }
        }
        break;
    case 3:
    case 9:
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
        behave(actor, 1);
        break;
    case 4:
        animation = walking;
        turnBy = turnTowards(body, hero, c.turning, milliseconds);
        if (heroAlive && distance <= c.sight)
            behave(actor, 2);
        break;
    case 7:
    case 8:
        animation = c.hurt;
        break;
    case 0xD:
        animation = 0x11;
        break;
    case 0xE:
        animation = 0x13;
        break;
    case 0xF:
        animation = 0x12;
        break;
    case 0x10:
        animation = 0x14;
        break;
    case 0x11: {
        animation = walking;
        turnBy = turnTowards(body, actor.goal, c.turning, milliseconds);
        actor.goingFor += milliseconds;
        const float dx = actor.goal[0] - body.x, dy = actor.goal[1] - body.y;
        if (dx * dx + dy * dy < kGoalReached * kGoalReached || actor.goingFor > kGoalGiveUp)
            behave(actor, photosaureChase(actor));
        break;
    }
    case 0x12:
        animation = 4;
        turnBy = turnTowards(body, hero, c.turning, milliseconds);
        if (inLight)
            behave(actor, photosaureFlight());
        break;
    case 0x13:
        animation = 6;
        turnBy = turnTowards(body, hero, c.turning, milliseconds);
        break;
    case 0x15:
        animation = 0xB;
        turnBy = turnTowards(body, hero, c.turning, milliseconds);
        break;
    case 0x16:
        animation = 10;
        turnBy = turnTowards(body, hero, c.turning, milliseconds);
        break;
    default:  // 0, 5 (not done), 6, 10, 0x84...
        break;
    }
}

// The PHOTOSAURE's brain once its animation has ended (over) this frame
void photosaureAfter(LevelActor& actor, bool over)
{
    if (!over || actor.behaviourFresh)
        return;
    switch (actor.behaviour) {
    case 1:
        behave(actor, 4);
        break;
    case 7:
    case 8:
        if (actor.life < 1)
            actor.presence = ActorPresence::Dying;
        else
            behave(actor, 2);
        break;
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x10:
    case 0x12:
    case 0x13:
        behave(actor, photosaureRecovery());
        break;
    case 0x15:
    case 0x16:
        behave(actor, photosaureChase(actor));
        break;
    default:
        break;
    }
}

} // namespace

int updateActor(LevelActor& actor, const SkeletonModel& model, const float hero[3], bool heroAlive,
                float milliseconds, const CollisionMap* collision, const NpcCharacteristics* npc,
                std::vector<int>* sounds, bool inLight)
{
    if (actor.life < 0)
        actor.life = npc && !npc->name.empty() ? npc->life() : 1;
    const ActorClass c = actorClass(actor.kind);
    CharacterMover& body = actor.body;
    if (actor.walkVariant < 0)
        actor.walkVariant = c.randomWalk && (std::rand() & 1) ? 25 : 0;
    const bool photosaure = actor.kind == 0;

    // Dying (order 5, FUN_0045c01d): its animation 5, then it is done and
    // hidden
    if (actor.presence == ActorPresence::Dying) {
        const int dying = animationWithIdentifier(model, c.dying);
        const AnimationClock clock =
            dying >= 0 ? moveBody(body, model, dying, milliseconds, 0, collision) : AnimationClock::Ended;
        if (clock != AnimationClock::Running) {
            actor.presence = ActorPresence::Done;
            actor.shown = false;
        }
        if (sounds && dying >= 0)
            for (const AnimationEvent& e : body.events)
                if (e.code == 201)
                    actorEventSounds(e.value, *sounds);
        return 0;
    }

    const float distance = distanceTo(body, hero);
    int animation = c.standing;
    float turnBy = 0;
    const bool fresh = actor.behaviourFresh;
    if (photosaure)
        photosaureChooses(actor, c, hero, heroAlive, inLight, milliseconds, animation, turnBy);
    else
        actor.behaviourFresh = false;
    switch (photosaure ? -1 : actor.behaviour) {
    case 1:
        // Waits until it sees the hero (FUN_00452277), then goes for it the
        // frame after
        if (!fresh)
            behave(actor, 4);
        else if (distance > c.sight)
            actor.behaviourFresh = true;  // still waiting
        break;
    case 2:
        animation = c.attacking;
        turnBy = turnTowards(body, hero, c.turning, milliseconds);
        break;
    case 3:
    case 9:
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
        behave(actor, 1);
        break;
    case 4:
        animation = c.walking + actor.walkVariant;
        if (animationWithIdentifier(model, animation) < 0)
            animation = c.walking;
        turnBy = turnTowards(body, hero, c.turning, milliseconds);
        if (distance <= c.reach && heroAlive)
            behave(actor, 2);
        break;
    case 7:
    case 8:
        animation = c.hurt;
        break;
    default:  // 0, 5 (not done), 6, 10...
        break;
    }

    int wanted = animationWithIdentifier(model, animation);
    if (wanted < 0)
        wanted = animationWithIdentifier(model, c.standing);
    if (wanted < 0 && !model.animations.empty())
        wanted = 0;
    const AnimationClock clock = moveBody(body, model, wanted, milliseconds, turnBy, collision);
    const bool over = clock != AnimationClock::Running;
    keepApart(body, hero);

    // Its blows (FUN_00452b31): a moment of code 1 hurts the hero if it is
    // within its value (cm) and ahead, with the damage of its attack
    // (NpcCarac.csv; the first one, +0x254 = 0)
    int damage = 0;
    for (const AnimationEvent& e : body.events) {
        if (e.code == 1 && heroAlive && distanceTo(body, hero) <= float(e.value) && heroAhead(body, hero))
            damage += npc && !npc->name.empty() ? std::max(npc->firstAttackDamage(), 0) : 0;
        if (e.code == 201 && sounds)
            actorEventSounds(e.value, *sounds);
    }

    // What waits for the end of an animation
    if (photosaure) {
        photosaureAfter(actor, over || wanted < 0);
        return damage;
    }
    if (actor.behaviour == 2 && over) {
        if (distance > c.reach)
            behave(actor, 4);
        else
            body.animation = -1;  // still within reach: the attack starts again
    }
    if ((actor.behaviour == 7 || actor.behaviour == 8) && (over || wanted < 0)) {
        if (actor.life < 1)
            actor.presence = ActorPresence::Dying;
        else
            behave(actor, 4);
    }
    // A dead hero is not attacked any more: it goes back to waiting
    if (!heroAlive && actor.behaviour == 2)
        behave(actor, 1);
    return damage;
}

int aimedActor(const std::vector<LevelActor>& actors, const CharacterMover& hero, float range, float cone)
{
    int best = -1;
    float bestDistance = range;
    for (size_t k = 0; k < actors.size(); k++) {
        const LevelActor& a = actors[k];
        if (a.presence != ActorPresence::Here)
            continue;
        const float dx = a.body.x - hero.x, dy = a.body.y - hero.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance > bestDistance)
            continue;
        const float towards = std::atan2(-dx, dy);
        if (std::fabs(std::remainder(towards - hero.heading, 2 * kPi)) > cone)
            continue;
        best = int(k);
        bestDistance = distance;
    }
    return best;
}

void hurtActor(LevelActor& actor, int damage, const NpcCharacteristics* npc)
{
    if (actor.presence != ActorPresence::Here)
        return;
    if (actor.life < 0)
        actor.life = npc && !npc->name.empty() ? npc->life() : 1;
    actor.life -= damage;
    behave(actor, 7);
}
