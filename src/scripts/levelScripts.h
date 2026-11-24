#pragma once

// The scripts of a level: several machines at once (a script can wait for
// frames), the game's variables they share (0x418 bytes) and the state of the
// game (both kept from level to level: what has been done in the game), and
// the 203 functions of the game they call (table at 0x4C7878, FUN_004667ac;
// docs/SCRIPTS.md says what each does and which are done). A script that
// calls one not done yet goes on as if it had done nothing, and the number is
// noted (missingFunctions).
//
// The game hands scripts pointers to its objects (the game, the state of the
// game, the level, the character, the things of the level) and they give them
// back to other functions; here they get handles instead (a kind and a
// number), which those functions understand.

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "inventory.h"
#include "levelLogic.h"
#include "scriptMachine.h"

// The state of the game the scripts change (the game's object at
// game + 0x100, FUN_00432020): kept from level to level
struct GameState {
    uint8_t levels[256] = {};      // per level (+0x274): low 4 bits a value, 0x10 lit, 0x20 can be lit
    uint8_t story[16] = {};        // bits of what has happened (+0x1080; functions 179, 180)
    uint8_t done[16] = {};         // bits: numbers the ranked list takes no more (+0x103C)
    int32_t pair[2] = {};          // +0x1074, +0x1078 (functions 161, 162)
    int life = 100;                // the hero's life points (+0xC; 100 at most, FUN_00435338)
    // A list of up to 16 numbers ranked by a value (+0x104C; functions 9, 10, 27)
    std::vector<std::pair<uint8_t, uint8_t>> ranked;
    // What the hero carries (functions 13, 23, 24, 25, 160, 164, 165)
    Inventory inventory;
    // The game's timers (game +0x1E1C and +0x1E5C: two banks of 8; functions
    // 88/89 set one, 6/7 read it, 14/15 stop it): milliseconds left (-1:
    // free) and the numbered script started when they run out (FUN_0044e12b)
    struct Timer {
        int32_t milliseconds = -1;
        int32_t script = 0;
    };
    Timer timers[2][8];
};

class LevelScripts {
public:
    LevelScripts();

    // The level whose scripts run (its DRAM block, texts, collision map...),
    // and its number. Stops the scripts that were running and any question
    // or message; the game's variables and state stay. nullptr: none
    void setLevel(LevelLogic* level, int number);

    // The camera shown now (function 87 tells it; FUN_00449af1)
    void setShownCamera(int camera) { shownCamera_ = camera; }

    // Starts the script at a place of the level's block on a free machine
    // (FUN_004629a6); nothing if there is none free or no script there
    void startScript(uint32_t script, const std::vector<int32_t>& parameters = {});

    // Starts the level's script with that number (FUN_0044a1b7). False if it
    // has none
    bool startEvent(int number, const std::vector<int32_t>& parameters = {});

    // Runs every machine that has a script for this frame; milliseconds since
    // the frame before
    void runFrame(float milliseconds);

    // What scripts asked the viewer for (-1: nothing); asking clears it:
    // a camera (functions 3, 199), a level to go to (4), a marked place of
    // the level to put the character at (72)
    int takeCameraRequest();
    int takeLevelRequest();
    int takeMarkerRequest();

    // Sounds the scripts asked for (functions 45, 47, 48), for the viewer to
    // play; asking clears them. A sound is a slot of the level's bank (0),
    // played as a note (60: its own pitch) with a left and right volume
    // (0..127); the handle is the game's (bank << 16 | channel, FUN_0049a4dd)
    struct SoundRequest {
        enum Kind { Play, Stop, StopAll } kind = Play;
        int32_t handle = -1;
        int slot = 0, note = 60, left = 127, right = 127;
    };
    std::vector<SoundRequest> takeSoundRequests();
    void requestSound(const SoundRequest& request) { sounds_.push_back(request); }
    int nextSoundChannel() { return soundChannel_++ % 16; }

    // The weapons of the hero (cweap.csv, aweap.csv): taking their
    // ammunition counts by them. Kept by the caller
    void setWeapons(const std::vector<WeaponCharacteristics>* weapons) { weapons_ = weapons; }
    const std::vector<WeaponCharacteristics>& weapons() const;

    // What the scripts asked of the music and the ambient sounds (functions
    // 93: load a file of midi\; 94: play a sequence by its name on a channel
    // of the state, its number given to the script; 51: its section; 52:
    // stop it; 53: its volume; 95: move its volume in some seconds), for the
    // viewer; asking clears them
    struct MusicRequest {
        enum Kind { Load, Play, Section, Stop, Volume, Fade } kind = Load;
        int channel = -1;
        std::string name;
        int a = 0, b = 0, c = 0, d = 0;  // Play: left, right, section, repeats; Section: section,
                                         // repeats; Volume/Fade: left, right (, seconds)
    };
    std::vector<MusicRequest> takeMusicRequests();
    void requestMusic(const MusicRequest& request) { music_.push_back(request); }
    // The channel of a sequence (the one with its name, else a free one;
    // FUN_00433122), -1 if none is free
    int musicChannel(const std::string& sequence);
    // A channel stopped (52), or all at a new game, are free again
    void freeMusicChannel(int channel);
    void freeMusicChannels() { musicChannels_.clear(); }

    // Items the hero has just taken with the game telling it (function 23:
    // the game shows "You take..." from item\IT3D_N), for the viewer; asking
    // clears them. Each: the item and how many
    std::vector<InventoryItem> takePickups();
    void notePickup(int item, int count) { pickups_.push_back({ item, count }); }

    // The effects the scripts asked for, for the viewer; asking clears them:
    // a particle system of the level shown for some milliseconds (function
    // 117: the thing's number, -1 always; FUN_0044f81d), the rain on or off
    // (105, FUN_0044a179) and a flash of light over the whole scene (174:
    // its strength 0..255 for 100 milliseconds, FUN_00423059: lightning)
    struct EffectRequest {
        enum Kind { ShowThing, Rain, Flash } kind = ShowThing;
        int a = 0, b = 0;  // ShowThing: thing, milliseconds; Rain: on; Flash: strength
    };
    std::vector<EffectRequest> takeEffectRequests();
    void requestEffect(const EffectRequest& request) { effects_.push_back(request); }

    // What the scripts asked of the hero, for the viewer; asking clears them:
    // led by the scripts (function 124: state 3, FUN_00437d10) or by the
    // player again (77, FUN_00436037), and the buttons held for it while led
    // (1, FUN_00437ebc: 1 forward, 2 back, 4 run)
    struct HeroRequest {
        enum Kind { Lead, Free, Buttons } kind = Lead;
        uint32_t buttons = 0;
    };
    std::vector<HeroRequest> takeHeroRequests();
    void requestHero(const HeroRequest& request) { hero_.push_back(request); }

    // A scene of the radio (function 158: its file, "\radio\76", and the
    // numbered script to start when it is over), for the viewer; asking
    // clears it (empty name: none)
    struct RadioRequest {
        std::string name;
        int script = -1;
    };
    RadioRequest takeRadioRequest();
    void requestRadio(const RadioRequest& request) { radio_ = request; }

    // Numbers of the functions of the game that scripts called and are not
    // done yet, as they were first called
    const std::vector<int>& missingFunctions() const { return missing_; }

    // A question on the screen (function 108): a text of the level and its
    // answers (another text split at each @: "Yes@No"). The script waits until
    // it is answered (number of the answer, from 0)
    struct Question {
        std::string text;
        std::vector<std::string> answers;
    };
    const Question* question() const { return asking_ ? &question_ : nullptr; }
    void answer(int chosen);

    // A full-screen picture (functions 8, 63: "\doc\101_51_A", a VRM of
    // the data): the game stops while it shows, for some milliseconds (-1:
    // until a key; the viewer counts them) and the script goes on once it
    // is closed
    struct Document {
        std::string name;
        int milliseconds = -1;
    };
    const Document* document() const { return showingDocument_ && !documentClosed_ ? &document_ : nullptr; }
    // A video (function 42: its number in MOVIES.TXT): the script waits until
    // it is over (-1: none asked)
    int movie() const { return showingMovie_ && !movieClosed_ ? movie_ : -1; }
    void closeMovie() { movieClosed_ = true; }
    bool viewMovie(int number);  // true once over
    void closeDocument() { documentClosed_ = true; }
    bool viewDocument(const std::string& name, int milliseconds);  // true once closed

    // A message on the screen (functions 107, 109, 154): the script goes on at
    // once; it stays until closed
    const std::string* message() const { return showingMessage_ ? &message_ : nullptr; }
    void closeMessage() { showingMessage_ = false; }

    // For the functions of the game (scripts/levelScripts.cpp)
    void requestCamera(int camera) { cameraRequest_ = camera; }
    void requestLevel(int level) { levelRequest_ = level; }
    void requestMarker(int marker) { markerRequest_ = marker; }
    int ask(int text, int answers);  // the answer once given, -1 while waiting
    void showMessage(int text);
    const std::string& text(int number) const;
    GameState& state() { return state_; }
    // The game variables the scripts share (0x418 bytes; kept with a saved game)
    std::vector<uint8_t>& variables() { return variables_; }
    const GameState& state() const { return state_; }
    LevelLogic* level() { return level_; }
    int levelNumber() const { return levelNumber_; }
    int shownCamera() const { return shownCamera_; }

private:
    static void noteMissing(int number, void* context);

    LevelLogic* level_ = nullptr;
    int levelNumber_ = -1;
    int shownCamera_ = -1;
    std::vector<uint8_t> variables_;
    GameState state_;
    std::vector<std::unique_ptr<ScriptMachine>> machines_;
    std::vector<ScriptNative> functions_;
    std::vector<int> missing_;
    int cameraRequest_ = -1, levelRequest_ = -1, markerRequest_ = -1;
    std::vector<SoundRequest> sounds_;
    int soundChannel_ = 0;
    const std::vector<WeaponCharacteristics>* weapons_ = nullptr;
    std::vector<InventoryItem> pickups_;
    std::vector<HeroRequest> hero_;
    std::vector<EffectRequest> effects_;
    RadioRequest radio_;
    std::vector<MusicRequest> music_;
    std::vector<std::string> musicChannels_;  // the sequence of each channel ("" free)
    Question question_;
    bool asking_ = false;
    int answer_ = -1;
    std::string message_;
    bool showingMessage_ = false;
    Document document_;
    bool showingDocument_ = false, documentClosed_ = false;
    int movie_ = -1;
    bool showingMovie_ = false, movieClosed_ = false;
};
