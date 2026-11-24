#include "levelScripts.h"

#include <algorithm>
#include <cstdlib>

namespace {

constexpr size_t kFunctionCount = 0xCB;     // table at 0x4C7878
constexpr size_t kStackSize = 0xA00;        // FUN_004667ac
constexpr size_t kGameVariables = 0x418;
constexpr size_t kMachineCount = 16;
constexpr size_t kRankedMost = 16;

// Handles for the objects of the game: kind in the top byte, a number below
enum HandleKind : uint32_t {
    kGameHandle = 1,   // the game (function 26)
    kStateHandle,      // the state of the game (22)
    kLevelHandle,      // the level (33)
    kHeroHandle,       // the character the player leads (71)
    kListHandle = 8,   // + a LevelList: a thing of one of the level's lists
};

int32_t handle(uint32_t kind, int number = 0)
{
    return int32_t((kind << 24) | (uint32_t(number) & 0xFFFFFFu));
}

LevelScripts& scriptsOf(void* context)
{
    return *static_cast<LevelScripts*>(context);
}

uint8_t& levelByte(LevelScripts& s, int level)
{
    static uint8_t none = 0;
    return level >= 0 && level < 256 ? s.state().levels[level] : (none = 0);
}

constexpr uint8_t kLevelLit = 0x10, kLevelCanBeLit = 0x20;

bool levelLit(LevelScripts& s, int level)
{
    const uint8_t b = levelByte(s, level);
    return (b & kLevelCanBeLit) != 0 && (b & kLevelLit) != 0;
}

void setBit(uint8_t& b, uint8_t bit, bool on)
{
    b = on ? uint8_t(b | bit) : uint8_t(b & ~bit);
}

// ---------------------------------------------------------------------------
// The functions of the game (numbers of the table at 0x4C7878; docs/SCRIPTS.md)

// 3, 199: show the camera of argument 0 (FUN_00462d39, FUN_004665b4)
NativeResult setCamera(ScriptMachine& m, void* c)
{
    scriptsOf(c).requestCamera(m.argument(0));
    return NativeResult::Continue;
}

// 4: go to the level of argument 0 (FUN_0044dcef)
NativeResult goToLevel(ScriptMachine& m, void* c)
{
    scriptsOf(c).requestLevel(m.argument(0));
    return NativeResult::Continue;
}

// 20: a random number from 0 to argument 0 - 1, in the slot after it
NativeResult randomNumber(ScriptMachine& m, void*)
{
    const int32_t below = m.argument(0);
    m.setArgument(4, below > 0 ? std::rand() % below : 0);
    return NativeResult::Continue;
}

// 22, 26, 33, 71: the state of the game, the game, the level, the character
NativeResult stateObject(ScriptMachine& m, void*)
{
    m.setArgument(0, handle(kStateHandle));
    return NativeResult::Continue;
}

NativeResult gameObject(ScriptMachine& m, void*)
{
    m.setArgument(0, handle(kGameHandle));
    return NativeResult::Continue;
}

NativeResult levelObject(ScriptMachine& m, void*)
{
    m.setArgument(0, handle(kLevelHandle));
    return NativeResult::Continue;
}

NativeResult heroObject(ScriptMachine& m, void*)
{
    m.setArgument(0, handle(kHeroHandle));
    return NativeResult::Continue;
}

// 65, 37, 166, 116, 129: thing number argument 0 of a list of the level (0
// for none), in the slot after it
template <LevelList list> NativeResult listThing(ScriptMachine& m, void* c)
{
    LevelLogic* level = scriptsOf(c).level();
    const int32_t number = m.argument(0);
    const bool known = level && number >= 0 && number < level->listSizes[list];
    m.setArgument(4, known ? handle(kListHandle + list, number) : 0);
    return NativeResult::Continue;
}

// 31, 32: the value (low 4 bits) of level argument 4 (FUN_00432033, FUN_00432076)
NativeResult setLevelValue(ScriptMachine& m, void* c)
{
    uint8_t& b = levelByte(scriptsOf(c), m.argument(4));
    b = uint8_t((b & 0xF0) | uint8_t(m.argument(8)));
    return NativeResult::Continue;
}

NativeResult levelValue(ScriptMachine& m, void* c)
{
    m.setArgument(8, levelByte(scriptsOf(c), m.argument(4)) & 0x0F);
    return NativeResult::Continue;
}

// 67 to 70: whether level argument 0 is lit, can be lit (FUN_004320e7,
// FUN_00432096, FUN_0043217f, FUN_0043212e). (Setting them for the level shown
// also makes the game redo its lights and camera: not done yet.)
NativeResult isLevelLit(ScriptMachine& m, void* c)
{
    m.setArgument(4, levelLit(scriptsOf(c), m.argument(0)) ? 1 : 0);
    return NativeResult::Continue;
}

NativeResult setLevelLit(ScriptMachine& m, void* c)
{
    setBit(levelByte(scriptsOf(c), m.argument(0)), kLevelLit, m.argument(4) != 0);
    return NativeResult::Continue;
}

NativeResult canLevelBeLit(ScriptMachine& m, void* c)
{
    m.setArgument(4, (levelByte(scriptsOf(c), m.argument(0)) & kLevelCanBeLit) != 0 ? 1 : 0);
    return NativeResult::Continue;
}

NativeResult setLevelCanBeLit(ScriptMachine& m, void* c)
{
    setBit(levelByte(scriptsOf(c), m.argument(0)), kLevelCanBeLit, m.argument(4) != 0);
    return NativeResult::Continue;
}

// 34, 35: whether the level shown is lit, and switching its light (FUN_00449a6b,
// FUN_0044bd50; argument 0 the level)
NativeResult isThisLevelLit(ScriptMachine& m, void* c)
{
    LevelScripts& s = scriptsOf(c);
    m.setArgument(4, levelLit(s, s.levelNumber()) ? 1 : 0);
    return NativeResult::Continue;
}

NativeResult switchThisLevelLight(ScriptMachine&, void* c)
{
    LevelScripts& s = scriptsOf(c);
    setBit(levelByte(s, s.levelNumber()), kLevelLit, !levelLit(s, s.levelNumber()));
    return NativeResult::Continue;
}

// 179, 180: a bit of what has happened in the story (FUN_0043298f, FUN_004329cf)
NativeResult storyBit(ScriptMachine& m, void* c)
{
    const int32_t bit = m.argument(4);
    const bool on = bit >= 0 && bit < 128 && (scriptsOf(c).state().story[bit >> 3] & (1 << (bit & 7))) != 0;
    m.setArgument(8, on ? 1 : 0);
    return NativeResult::Continue;
}

NativeResult setStoryBit(ScriptMachine& m, void* c)
{
    const int32_t bit = m.argument(4);
    if (bit >= 0 && bit < 128)
        scriptsOf(c).state().story[bit >> 3] |= uint8_t(1 << (bit & 7));
    return NativeResult::Continue;
}

// 13, 23: take an item (argument 4; the state is argument 0), one of it;
// 23 also shows it taken ("You take...", FUN_0044d543) and the hero
// reacts (FUN_0043540e, not done)
NativeResult takeOneItem(ScriptMachine& m, void* c)
{
    LevelScripts& s = scriptsOf(c);
    takeItem(s.state().inventory, m.argument(4), 1, s.weapons());
    return NativeResult::Continue;
}

NativeResult pickUpItem(ScriptMachine& m, void* c)
{
    LevelScripts& s = scriptsOf(c);
    takeItem(s.state().inventory, m.argument(4), 1, s.weapons());
    s.notePickup(m.argument(4), 1);
    return NativeResult::Continue;
}

// 182: take some of an item (argument 8, at least 1) and say so, as 23
NativeResult pickUpItems(ScriptMachine& m, void* c)
{
    LevelScripts& s = scriptsOf(c);
    const int count = std::max(m.argument(8), 1);
    takeItem(s.state().inventory, m.argument(4), count, s.weapons());
    s.notePickup(m.argument(4), count);
    return NativeResult::Continue;
}

// 8, 63: a full-screen picture (FUN_00487038): 8 with its name at 8 and
// how long at 0x88 (milliseconds, -1 until a key; the game fades it in and
// out, FUN_00418845, FUN_004a456b); 63 its name at 8 until a key (and the
// hero reacts, FUN_0043540e, not done). The script waits until it is closed
NativeResult showPicture(ScriptMachine& m, void* c)
{
    return scriptsOf(c).viewDocument(m.textArgument(8), m.argument(0x88)) ? NativeResult::Continue
                                                                          : NativeResult::Retry;
}

NativeResult showPictureUntilKey(ScriptMachine& m, void* c)
{
    return scriptsOf(c).viewDocument(m.textArgument(8), -1) ? NativeResult::Continue : NativeResult::Retry;
}

// 42: a video by its number in MOVIES.TXT (argument 0; 4: how it may be
// skipped, FUN_004811cf); the hero reacts (FUN_0043540e, not done) and the
// script waits until it is over (FUN_00463908)
NativeResult playMovie(ScriptMachine& m, void* c)
{
    return scriptsOf(c).viewMovie(m.argument(0)) ? NativeResult::Continue : NativeResult::Retry;
}

// 124, 77: the hero (argument 0) led by the scripts (FUN_00437d10) or by
// the player again (FUN_00436037)
template <bool Lead>
NativeResult leadHero(ScriptMachine&, void* c)
{
    LevelScripts::HeroRequest r;
    r.kind = Lead ? LevelScripts::HeroRequest::Lead : LevelScripts::HeroRequest::Free;
    scriptsOf(c).requestHero(r);
    return NativeResult::Continue;
}

// 1: the buttons held for the hero (argument 0) while the scripts lead it:
// forward (4), back (8), run (0xC) (FUN_00437ebc)
NativeResult holdHeroButtons(ScriptMachine& m, void* c)
{
    LevelScripts::HeroRequest r;
    r.kind = LevelScripts::HeroRequest::Buttons;
    r.buttons = (m.argument(4) ? 1u : 0u) | (m.argument(8) ? 2u : 0u) | (m.argument(0xC) ? 4u : 0u);
    scriptsOf(c).requestHero(r);
    return NativeResult::Continue;
}

// 117: the particle system thing argument 0 (of function 116) shown for
// argument 4 milliseconds (-1 always, 0 no more; FUN_0044f81d)
NativeResult showThing(ScriptMachine& m, void* c)
{
    const int32_t h = m.argument(0);
    if ((uint32_t(h) >> 24) == kListHandle + kListThings) {
        LevelScripts::EffectRequest r;
        r.kind = LevelScripts::EffectRequest::ShowThing;
        r.a = int(uint32_t(h) & 0xFFFFFFu);
        r.b = m.argument(4);
        scriptsOf(c).requestEffect(r);
    }
    return NativeResult::Continue;
}

// 105: the rain of the level (argument 0) on or off (argument 4; FUN_0044a179)
NativeResult setRain(ScriptMachine& m, void* c)
{
    LevelScripts::EffectRequest r;
    r.kind = LevelScripts::EffectRequest::Rain;
    r.a = m.argument(4) != 0;
    scriptsOf(c).requestEffect(r);
    return NativeResult::Continue;
}

// 174: a flash over the whole scene, of strength argument 0 x 2 (up to 255)
// for 100 milliseconds (FUN_004194b5, FUN_00423059): the lightning
NativeResult flash(ScriptMachine& m, void* c)
{
    LevelScripts::EffectRequest r;
    r.kind = LevelScripts::EffectRequest::Flash;
    r.a = std::clamp(m.argument(0) * 2, 0, 255);
    scriptsOf(c).requestEffect(r);
    return NativeResult::Continue;
}

// 158: a scene of the radio (its file at 0, "\radio\76"; the numbered script
// to start when it is over at 0x80; FUN_0044eaa9)
NativeResult startRadio(ScriptMachine& m, void* c)
{
    LevelScripts::RadioRequest r;
    r.name = m.textArgument(0);
    r.script = m.argument(0x80);
    scriptsOf(c).requestRadio(r);
    return NativeResult::Continue;
}

// 24, 25: drop an item; whether it has one (result at 8)
NativeResult dropOneItem(ScriptMachine& m, void* c)
{
    dropItem(scriptsOf(c).state().inventory, m.argument(4));
    return NativeResult::Continue;
}

NativeResult hasOneItem(ScriptMachine& m, void* c)
{
    m.setArgument(8, hasItem(scriptsOf(c).state().inventory, m.argument(4)) ? 1 : 0);
    return NativeResult::Continue;
}

// 160: note a number once (FUN_00448d7e)
NativeResult noteItemNumber(ScriptMachine& m, void* c)
{
    noteNumber(scriptsOf(c).state().inventory, m.argument(4));
    return NativeResult::Continue;
}

// 164, 165: set how many of an item; read it (result at 8)
NativeResult setOneItemCount(ScriptMachine& m, void* c)
{
    setItemCount(scriptsOf(c).state().inventory, m.argument(4), int16_t(m.argument(8)));
    return NativeResult::Continue;
}

NativeResult oneItemCount(ScriptMachine& m, void* c)
{
    m.setArgument(8, itemCount(scriptsOf(c).state().inventory, m.argument(4)));
    return NativeResult::Continue;
}

// 88, 89: start a timer of bank 0 or 1 (FUN_0044e2d4, FUN_0044e39d): the
// numbered script at argument 0, the milliseconds at 4; its number (0..7,
// -1 if all are in use) at 8
template <int Bank>
NativeResult startTimer(ScriptMachine& m, void* c)
{
    GameState::Timer* timers = scriptsOf(c).state().timers[Bank];
    int slot = -1;
    for (int k = 0; k < 8 && slot < 0; k++)
        if (timers[k].milliseconds < 0)
            slot = k;
    if (slot >= 0)
        timers[slot] = { m.argument(4), m.argument(0) };
    m.setArgument(8, slot);
    return NativeResult::Continue;
}

// 6, 7: the milliseconds left of timer N (0 if run out or free;
// FUN_0044e335, FUN_0044e3fe), at 4
template <int Bank>
NativeResult timerLeft(ScriptMachine& m, void* c)
{
    const int slot = m.argument(0);
    const int32_t left = slot >= 0 && slot < 8 ? scriptsOf(c).state().timers[Bank][slot].milliseconds : 0;
    m.setArgument(4, left < 1 ? 0 : left);
    return NativeResult::Continue;
}

// 14, 15: stop timer N (FUN_0044e371, FUN_0044e43a)
template <int Bank>
NativeResult stopTimer(ScriptMachine& m, void* c)
{
    const int slot = m.argument(0);
    if (slot >= 0 && slot < 8)
        scriptsOf(c).state().timers[Bank][slot].milliseconds = -1;
    return NativeResult::Continue;
}

// 93: load a file of midi\ (its name at 0: "\midi\carnby\jardind1")
NativeResult loadMusic(ScriptMachine& m, void* c)
{
    LevelScripts::MusicRequest r;
    r.kind = LevelScripts::MusicRequest::Load;
    r.name = m.textArgument(0);
    scriptsOf(c).requestMusic(r);
    return NativeResult::Continue;
}

// 94: play a sequence (its name at 0, left at 0x80, right at 0x84, its
// section at 0x88, how many times at 0x8C; FUN_004992f7) on a channel of the
// state, whose number goes at 0x90 (FUN_00433122)
NativeResult playMusic(ScriptMachine& m, void* c)
{
    LevelScripts& s = scriptsOf(c);
    LevelScripts::MusicRequest r;
    r.kind = LevelScripts::MusicRequest::Play;
    r.name = m.textArgument(0);
    r.channel = s.musicChannel(r.name);
    r.a = m.argument(0x80) & 0xFF;
    r.b = m.argument(0x84) & 0xFF;
    r.c = m.argument(0x88);
    r.d = m.argument(0x8C);
    if (r.channel >= 0)
        s.requestMusic(r);
    m.setArgument(0x90, r.channel);
    return NativeResult::Continue;
}

// 51, 52, 53, 95: on the channel at 0: its section and how many times (4, 8;
// FUN_00499589); stop it (FUN_00499624); its volume left and right (4, 8;
// FUN_004998eb); move it to left and right in some seconds (4, 8, 0xC;
// FUN_00499b26)
template <LevelScripts::MusicRequest::Kind Kind>
NativeResult musicChannelRequest(ScriptMachine& m, void* c)
{
    LevelScripts::MusicRequest r;
    r.kind = Kind;
    r.channel = m.argument(0);
    r.a = m.argument(4);
    r.b = m.argument(8);
    r.c = m.argument(0xC);
    if (r.channel >= 0) {
        scriptsOf(c).requestMusic(r);
        if (Kind == LevelScripts::MusicRequest::Stop)
            scriptsOf(c).freeMusicChannel(r.channel);
    }
    return NativeResult::Continue;
}

// 161, 162: two values of the state (FUN_004328d3, FUN_004328f8)
NativeResult setPair(ScriptMachine& m, void* c)
{
    scriptsOf(c).state().pair[0] = m.argument(4);
    scriptsOf(c).state().pair[1] = m.argument(8);
    return NativeResult::Continue;
}

NativeResult pairValue(ScriptMachine& m, void* c)
{
    m.setArgument(4, scriptsOf(c).state().pair[0]);
    return NativeResult::Continue;
}

// 9, 10, 27: the ranked list of the state (FUN_00432282, FUN_004323a2,
// FUN_0043243f): add a number with its rank (not if marked done, already
// there or full), take one out, empty it
NativeResult addRanked(ScriptMachine& m, void* c)
{
    GameState& state = scriptsOf(c).state();
    const int32_t rank = m.argument(8);
    const uint8_t id = uint8_t(m.argument(4));
    if ((state.done[id >> 3] & (1 << (id & 7))) != 0 || state.ranked.size() >= kRankedMost)
        return NativeResult::Continue;
    for (const auto& entry : state.ranked)
        if (entry.first == id)
            return NativeResult::Continue;
    size_t at = 0;
    while (at < state.ranked.size() && int32_t(state.ranked[at].second) <= rank)
        at++;
    state.ranked.insert(state.ranked.begin() + std::ptrdiff_t(at), { id, uint8_t(rank) });
    return NativeResult::Continue;
}

NativeResult removeRanked(ScriptMachine& m, void* c)
{
    auto& ranked = scriptsOf(c).state().ranked;
    const uint8_t id = uint8_t(m.argument(4));
    for (auto it = ranked.begin(); it != ranked.end(); ++it)
        if (it->first == id) {
            ranked.erase(it);
            break;
        }
    return NativeResult::Continue;
}

NativeResult clearRanked(ScriptMachine&, void* c)
{
    scriptsOf(c).state().ranked.clear();
    return NativeResult::Continue;
}

// The thing of the list of 0x9C (function 37) a handle stands for, or nullptr
LevelObject* objectOf(void* c, int32_t h)
{
    LevelLogic* level = scriptsOf(c).level();
    const uint32_t kind = uint32_t(h) >> 24, number = uint32_t(h) & 0xFFFFFFu;
    if (!level || kind != kListHandle + kListObjects || number >= level->objects.size())
        return nullptr;
    return &level->objects[number];
}

// 38: show the object argument 0 or not (argument 4; FUN_00468ad8: its model,
// its shadow and its other parts)
NativeResult showObject(ScriptMachine& m, void* c)
{
    if (LevelObject* object = objectOf(c, m.argument(0)))
        object->visible = m.argument(4) != 0;
    return NativeResult::Continue;
}

// 39: whether the object argument 0 is shown, in the slot after it
// (FUN_00468c1b)
NativeResult objectShown(ScriptMachine& m, void* c)
{
    const LevelObject* object = objectOf(c, m.argument(0));
    m.setArgument(4, object && object->visible ? 1 : 0);
    return NativeResult::Continue;
}

// 40: the object argument 0 plays animation argument 4 (FUN_00468ca7)
NativeResult animateObject(ScriptMachine& m, void* c)
{
    if (LevelObject* object = objectOf(c, m.argument(0))) {
        object->animation = m.argument(4);
        object->animating = true;
    }
    return NativeResult::Continue;
}

// 97: show the shadow of the object argument 0 or not (argument 4;
// FUN_00468c2f)
NativeResult showObjectShadow(ScriptMachine& m, void* c)
{
    if (LevelObject* object = objectOf(c, m.argument(0)))
        object->shadowVisible = m.argument(4) != 0;
    return NativeResult::Continue;
}

// The actor (function 65) a handle stands for, or nullptr
LevelActor* actorOf(void* c, int32_t h)
{
    LevelLogic* level = scriptsOf(c).level();
    const uint32_t kind = uint32_t(h) >> 24, number = uint32_t(h) & 0xFFFFFFu;
    if (!level || kind != kListHandle + kListActors || number >= level->actors.size())
        return nullptr;
    return &level->actors[number];
}

// The game leaves orders to an actor in a queue that it carries out on its
// turn (FUN_0045126c); here they are carried out at once, and the steps an
// actor goes through on its own end at once too (3, 4), except dying: its
// brain plays its death (core/actorBrain)
void orderActor(LevelActor& a, int order, int behaviour = -1)
{
    switch (order) {
    case 1:  // appear
    case 2:  // appear to act (3, then here)
        if (a.presence == ActorPresence::Away) {
            a.presence = ActorPresence::Here;
            a.shown = true;
        }
        break;
    case 3:  // go away
    case 4:  // go away another way (4, then away: FUN_004555ef)
        if (a.presence == ActorPresence::Here) {
            a.presence = ActorPresence::Away;
            a.shown = false;
        }
        break;
    case 5:  // die: its brain plays its animation 5, then it is done and hidden
        if (a.presence == ActorPresence::Here)
            a.presence = ActorPresence::Dying;
        break;
    case 6:  // start again, with a behaviour (FUN_00454ab4, FUN_00452586)
        if (a.presence == ActorPresence::Done) {
            a.presence = ActorPresence::Away;
            if (behaviour >= 0)
                a.behaviour = behaviour;
        }
        break;
    }
}

// 192: the actor argument 0 appears (argument 4: 0 just appears, else appears
// to act; FUN_0045148d)
NativeResult actorAppears(ScriptMachine& m, void* c)
{
    if (LevelActor* a = actorOf(c, m.argument(0)))
        orderActor(*a, m.argument(4) != 0 ? 2 : 1);
    return NativeResult::Continue;
}

// 193: the actor argument 0 goes away (argument 4 picks how; FUN_00451506)
NativeResult actorLeaves(ScriptMachine& m, void* c)
{
    if (LevelActor* a = actorOf(c, m.argument(0)))
        orderActor(*a, m.argument(4) != 0 ? 4 : 3);
    return NativeResult::Continue;
}

// 194: the actor argument 0 starts again (FUN_0045157f with -1: keeps its
// behaviour)
NativeResult actorRestarts(ScriptMachine& m, void* c)
{
    if (LevelActor* a = actorOf(c, m.argument(0)))
        orderActor(*a, 6);
    return NativeResult::Continue;
}

// 195: the actor argument 0 dies (FUN_004515c7)
NativeResult actorDies(ScriptMachine& m, void* c)
{
    if (LevelActor* a = actorOf(c, m.argument(0)))
        orderActor(*a, 5);
    return NativeResult::Continue;
}

// 196, 197, 198: whether the actor argument 0 is here, away or done, in the
// slot after it (FUN_00451601)
template <ActorPresence presence> NativeResult actorIs(ScriptMachine& m, void* c)
{
    const LevelActor* a = actorOf(c, m.argument(0));
    m.setArgument(4, a && a->presence == presence ? 1 : 0);
    return NativeResult::Continue;
}

// 66: the behaviour of the actor argument 0 (argument 4; FUN_00452586)
NativeResult setActorBehaviour(ScriptMachine& m, void* c)
{
    if (LevelActor* a = actorOf(c, m.argument(0))) {
        a->behaviour = m.argument(4);
        a->behaviourFresh = true;
    }
    return NativeResult::Continue;
}

// 45: play slot argument 0 of the level's sound bank as note argument 4,
// with volumes argument 12 (left) and 16 (right) (FUN_0049a4dd; argument 8
// is not used); its handle in the slot after them
NativeResult playSound(ScriptMachine& m, void* c)
{
    LevelScripts& s = scriptsOf(c);
    LevelScripts::SoundRequest r;
    r.kind = LevelScripts::SoundRequest::Play;
    r.handle = s.nextSoundChannel();  // bank 0
    r.slot = m.argument(0) & 0xFF;
    r.note = m.argument(4) & 0x7F;
    r.left = m.argument(12);
    r.right = m.argument(16);
    s.requestSound(r);
    m.setArgument(20, r.handle);
    return NativeResult::Continue;
}

// 47: stop the sound of handle argument 0
NativeResult stopSound(ScriptMachine& m, void* c)
{
    LevelScripts::SoundRequest r;
    r.kind = LevelScripts::SoundRequest::Stop;
    r.handle = m.argument(0);
    scriptsOf(c).requestSound(r);
    return NativeResult::Continue;
}

// 48: stop every sound
NativeResult stopSounds(ScriptMachine&, void* c)
{
    LevelScripts::SoundRequest r;
    r.kind = LevelScripts::SoundRequest::StopAll;
    scriptsOf(c).requestSound(r);
    return NativeResult::Continue;
}

// 36: turn group argument 4 of the collision map on or off (argument 8;
// FUN_00449c33): doors, gates...
NativeResult setCollisionGroupOn(ScriptMachine& m, void* c)
{
    if (LevelLogic* level = scriptsOf(c).level())
        setCollisionGroup(level->collision, m.argument(4), m.argument(8) != 0);
    return NativeResult::Continue;
}

// 43: wait argument 0 milliseconds (FUN_004639b3 counts frames of 60 a second)
NativeResult waitMilliseconds(ScriptMachine& m, void*)
{
    return m.waitedMilliseconds() < float(m.argument(0)) ? NativeResult::Retry : NativeResult::Continue;
}

// 75: wait for the next frame
NativeResult nextFrame(ScriptMachine&, void*)
{
    return NativeResult::Yield;
}

// 87: the camera shown (FUN_00449af1)
NativeResult cameraShown(ScriptMachine& m, void* c)
{
    m.setArgument(4, scriptsOf(c).shownCamera());
    return NativeResult::Continue;
}

// 90, 138 to 145: start the level's script numbered argument 0, with 0 to 8
// parameters after it (FUN_0044e104)
template <int count> NativeResult startNumbered(ScriptMachine& m, void* c)
{
    std::vector<int32_t> parameters;
    for (int k = 0; k < count; k++)
        parameters.push_back(m.argument(4 + 4 * k));
    scriptsOf(c).startEvent(m.argument(0), parameters);
    return NativeResult::Continue;
}

// 137: the parameter number argument 0 the script was started with (FUN_00411ae1)
NativeResult scriptParameter(ScriptMachine& m, void*)
{
    m.setArgument(4, m.parameter(m.argument(0)));
    return NativeResult::Continue;
}

// 72: put the character (argument 0) at marked place argument 4 (FUN_00434a0e)
NativeResult putAtMarker(ScriptMachine& m, void* c)
{
    scriptsOf(c).requestMarker(m.argument(4));
    return NativeResult::Continue;
}

// 128: whether none of the level's actors is still doing something
// (FUN_00449a93): the level has no actors yet
NativeResult actorsQuiet(ScriptMachine& m, void*)
{
    m.setArgument(4, 1);
    return NativeResult::Continue;
}

// 29, 49, 50, 54, 55, 83, 114: the game's do nothing; 28, 30 only put 0 in
// their result
NativeResult doNothing(ScriptMachine&, void*)
{
    return NativeResult::Continue;
}

NativeResult resultZero(ScriptMachine& m, void*)
{
    m.setArgument(8, 0);
    return NativeResult::Continue;
}

// 108: ask a question (texts argument 0 and 4), the answer after them
NativeResult askQuestion(ScriptMachine& m, void* c)
{
    const int chosen = scriptsOf(c).ask(m.argument(0), m.argument(4));
    if (chosen < 0)
        return NativeResult::Retry;
    m.setArgument(8, chosen);
    return NativeResult::Continue;
}

// 107, 109, 154: show text argument 0 of the level as a message (argument 4:
// how long or how, not studied; FUN_0043ce5f, FUN_0043d661)
NativeResult showLevelText(ScriptMachine& m, void* c)
{
    scriptsOf(c).showMessage(m.argument(0));
    return NativeResult::Continue;
}

} // namespace

LevelScripts::LevelScripts() : variables_(kGameVariables, 0), functions_(kFunctionCount, nullptr)
{
    for (size_t k = 0; k < kMachineCount; k++)
        machines_.push_back(std::make_unique<ScriptMachine>(kStackSize, &variables_));
    ScriptNative* f = functions_.data();
    f[3] = f[199] = setCamera;
    f[29] = f[49] = f[50] = f[54] = f[55] = f[83] = f[114] = doNothing;
    f[28] = f[30] = resultZero;
    f[4] = goToLevel;
    f[9] = addRanked;
    f[10] = removeRanked;
    f[20] = randomNumber;
    f[22] = stateObject;
    f[26] = gameObject;
    f[27] = clearRanked;
    f[31] = setLevelValue;
    f[32] = levelValue;
    f[33] = levelObject;
    f[34] = isThisLevelLit;
    f[35] = switchThisLevelLight;
    f[36] = setCollisionGroupOn;
    f[37] = listThing<kListObjects>;
    f[38] = showObject;
    f[39] = objectShown;
    f[40] = animateObject;
    f[43] = waitMilliseconds;
    f[65] = listThing<kListActors>;
    f[66] = setActorBehaviour;
    f[67] = isLevelLit;
    f[68] = setLevelLit;
    f[69] = canLevelBeLit;
    f[70] = setLevelCanBeLit;
    f[71] = heroObject;
    f[72] = putAtMarker;
    f[75] = nextFrame;
    f[87] = cameraShown;
    f[90] = startNumbered<0>;
    f[107] = f[109] = f[154] = showLevelText;
    f[108] = askQuestion;
    f[97] = showObjectShadow;
    f[116] = listThing<kListThings>;
    f[128] = actorsQuiet;
    f[129] = listThing<kListOthers>;
    f[137] = scriptParameter;
    f[138] = startNumbered<1>;
    f[139] = startNumbered<2>;
    f[140] = startNumbered<3>;
    f[141] = startNumbered<4>;
    f[142] = startNumbered<5>;
    f[143] = startNumbered<6>;
    f[144] = startNumbered<7>;
    f[145] = startNumbered<8>;
    f[93] = loadMusic;
    f[94] = playMusic;
    f[51] = musicChannelRequest<LevelScripts::MusicRequest::Section>;
    f[52] = musicChannelRequest<LevelScripts::MusicRequest::Stop>;
    f[53] = musicChannelRequest<LevelScripts::MusicRequest::Volume>;
    f[95] = musicChannelRequest<LevelScripts::MusicRequest::Fade>;
    f[88] = startTimer<0>;
    f[89] = startTimer<1>;
    f[6] = timerLeft<0>;
    f[7] = timerLeft<1>;
    f[14] = stopTimer<0>;
    f[15] = stopTimer<1>;
    f[42] = playMovie;
    f[124] = leadHero<true>;
    f[77] = leadHero<false>;
    f[1] = holdHeroButtons;
    f[158] = startRadio;
    f[117] = showThing;
    f[105] = setRain;
    f[174] = flash;
    f[8] = showPicture;
    f[63] = showPictureUntilKey;
    f[182] = pickUpItems;
    f[13] = takeOneItem;
    f[23] = pickUpItem;
    f[24] = dropOneItem;
    f[25] = hasOneItem;
    f[160] = noteItemNumber;
    f[164] = setOneItemCount;
    f[165] = oneItemCount;
    f[161] = setPair;
    f[162] = pairValue;
    f[166] = listThing<kListMovers>;
    f[179] = storyBit;
    f[180] = setStoryBit;
    f[45] = playSound;
    f[47] = stopSound;
    f[48] = stopSounds;
    f[192] = actorAppears;
    f[193] = actorLeaves;
    f[194] = actorRestarts;
    f[195] = actorDies;
    f[196] = actorIs<ActorPresence::Here>;
    f[197] = actorIs<ActorPresence::Away>;
    f[198] = actorIs<ActorPresence::Done>;
}

const std::vector<WeaponCharacteristics>& LevelScripts::weapons() const
{
    static const std::vector<WeaponCharacteristics> none;
    return weapons_ ? *weapons_ : none;
}

std::vector<LevelScripts::MusicRequest> LevelScripts::takeMusicRequests()
{
    std::vector<MusicRequest> taken;
    taken.swap(music_);
    return taken;
}

void LevelScripts::freeMusicChannel(int channel)
{
    if (channel >= 0 && size_t(channel) < musicChannels_.size())
        musicChannels_[size_t(channel)].clear();
}

int LevelScripts::musicChannel(const std::string& sequence)
{
    constexpr size_t kChannels = 8;  // as the music player's slots
    musicChannels_.resize(kChannels);
    for (size_t k = 0; k < kChannels; k++)
        if (musicChannels_[k] == sequence)
            return int(k);
    for (size_t k = 0; k < kChannels; k++)
        if (musicChannels_[k].empty()) {
            musicChannels_[k] = sequence;
            return int(k);
        }
    return -1;
}

std::vector<LevelScripts::EffectRequest> LevelScripts::takeEffectRequests()
{
    std::vector<EffectRequest> out;
    out.swap(effects_);
    return out;
}

std::vector<LevelScripts::HeroRequest> LevelScripts::takeHeroRequests()
{
    std::vector<HeroRequest> out;
    out.swap(hero_);
    return out;
}

LevelScripts::RadioRequest LevelScripts::takeRadioRequest()
{
    RadioRequest out = radio_;
    radio_ = RadioRequest();
    return out;
}

std::vector<InventoryItem> LevelScripts::takePickups()
{
    std::vector<InventoryItem> taken;
    taken.swap(pickups_);
    return taken;
}

std::vector<LevelScripts::SoundRequest> LevelScripts::takeSoundRequests()
{
    std::vector<SoundRequest> taken;
    taken.swap(sounds_);
    return taken;
}

void LevelScripts::setLevel(LevelLogic* level, int number)
{
    level_ = level;
    levelNumber_ = number;
    for (auto& machine : machines_)
        machine = std::make_unique<ScriptMachine>(kStackSize, &variables_);
    cameraRequest_ = levelRequest_ = markerRequest_ = answer_ = -1;
    asking_ = showingMessage_ = showingDocument_ = documentClosed_ = showingMovie_ = movieClosed_ = false;
}

void LevelScripts::startScript(uint32_t script, const std::vector<int32_t>& parameters)
{
    if (!level_ || script == 0)
        return;
    for (auto& machine : machines_)
        if (!machine->busy()) {
            machine->start(level_->block.data(), level_->block.size(), script, parameters);
            return;
        }
}

bool LevelScripts::startEvent(int number, const std::vector<int32_t>& parameters)
{
    if (!level_)
        return false;
    for (const auto& event : level_->events)
        if (event.first == number) {
            startScript(event.second, parameters);
            return true;
        }
    return false;
}

void LevelScripts::runFrame(float milliseconds)
{
    // The timers (FUN_0044e12b): the ones that run out start their script
    for (auto& bank : state_.timers)
        for (GameState::Timer& timer : bank)
            if (timer.milliseconds >= 0) {
                timer.milliseconds -= int32_t(milliseconds);
                if (timer.milliseconds < 0)
                    startEvent(timer.script);
            }
    const ScriptNatives natives{ functions_.data(), functions_.size(), this, noteMissing };
    for (auto& machine : machines_)
        if (machine->busy())
            machine->run(natives, milliseconds);
}

int LevelScripts::takeCameraRequest()
{
    return std::exchange(cameraRequest_, -1);
}

int LevelScripts::takeLevelRequest()
{
    return std::exchange(levelRequest_, -1);
}

int LevelScripts::takeMarkerRequest()
{
    return std::exchange(markerRequest_, -1);
}

void LevelScripts::answer(int chosen)
{
    if (asking_)
        answer_ = chosen;
}

const std::string& LevelScripts::text(int number) const
{
    static const std::string none;
    return level_ && number >= 0 && size_t(number) < level_->texts.size() ? level_->texts[size_t(number)] : none;
}

bool LevelScripts::viewMovie(int number)
{
    if (showingMovie_ && movieClosed_) {
        showingMovie_ = movieClosed_ = false;
        return true;
    }
    if (!showingMovie_) {
        movie_ = number;
        showingMovie_ = true;
        movieClosed_ = false;
    }
    return false;
}

bool LevelScripts::viewDocument(const std::string& name, int milliseconds)
{
    if (showingDocument_ && documentClosed_) {
        showingDocument_ = documentClosed_ = false;
        return true;
    }
    if (!showingDocument_) {
        document_ = { name, milliseconds };
        showingDocument_ = true;
        documentClosed_ = false;
    }
    return false;
}

int LevelScripts::ask(int textNumber, int answers)
{
    if (asking_ && answer_ >= 0) {
        const int chosen = answer_;
        asking_ = false;
        answer_ = -1;
        return chosen;
    }
    if (!asking_) {
        question_.text = text(textNumber);
        question_.answers.clear();
        const std::string& all = text(answers);
        size_t start = 0;
        for (size_t at = all.find('@');; at = all.find('@', start)) {
            question_.answers.push_back(all.substr(start, at == std::string::npos ? std::string::npos : at - start));
            if (at == std::string::npos)
                break;
            start = at + 1;
        }
        asking_ = true;
        answer_ = -1;
    }
    return -1;
}

void LevelScripts::showMessage(int textNumber)
{
    message_ = text(textNumber);
    showingMessage_ = true;
}

void LevelScripts::noteMissing(int number, void* context)
{
    LevelScripts& scripts = *static_cast<LevelScripts*>(context);
    if (std::find(scripts.missing_.begin(), scripts.missing_.end(), number) == scripts.missing_.end())
        scripts.missing_.push_back(number);
}
