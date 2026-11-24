#pragma once

// The game's scenes of the radio (radio\N, started by function 158 of the
// scripts, FUN_0044eaa9; the game's object at +0x1EBC, FUN_00430d7a): Carnby
// or Aline take the radio out and talk with the other one, the game leading
// the hero and the camera while the voices of xa\ play.
//
// A file: a header of 16 bytes (+4: where the table of blocks starts, +8:
// how many blocks; both from 0x10) and blocks of orders. An order is its
// number (16 bits), and if it has 0x8000 the block to start when it is over
// (16 bits); then the size of its arguments (16 bits) and them (32 bits
// each). The scene starts block 0 (FUN_00431007): its orders run at once
// until one with a block to start, which waits: it becomes a watcher (16 at
// most), looked at every frame (FUN_00430dff) until it is over, and then its
// block starts. The scene ends when no watcher is left; then the numbered
// script given to function 158, if any, starts (FUN_0044da0a). The orders
// are the game's table at 0x4C5B68 (two functions each: start and "still
// going?"): docs/ALONE4_EXE.md.

#include <cstdint>
#include <string>
#include <vector>

// What the orders do in the game, done by the viewer
class RadioActions {
public:
    virtual ~RadioActions() = default;
    // 4: the camera to show, and the one shown
    virtual void requestCamera(int camera) = 0;
    virtual int shownCamera() const = 0;
    // 5, 6: the hero led by the scene (state 3, FUN_00437d10) or by the
    // player again (FUN_00436037)
    virtual void leadHero(bool byScene) = 0;
    // 7: the buttons the scene holds for the hero (FUN_00437ebc: 1 forward,
    // 2 back, 4 run); an animation asked for stops
    virtual void holdHeroButtons(uint32_t buttons) = 0;
    // 8: the hero goes to a marked place of the level (FUN_00437f7d), until
    // it gets there (FUN_00438000)
    virtual void walkHeroTo(int marker) = 0;
    virtual bool heroWalking() const = 0;
    // 9: the hero turns to face a marked place in some milliseconds
    // (FUN_00438014, FUN_00438147)
    virtual void turnHeroTo(int marker, int milliseconds) = 0;
    virtual bool heroTurning() const = 0;
    // 10: the hero plays an animation (an identifier, FUN_0043838f) until
    // its end; whether its moves place the hero (two flags of placeHero)
    virtual void playHeroAnimation(int identifier, bool moveAlong, bool turnAlong) = 0;
    virtual bool heroAnimationPlaying() const = 0;
    // 12: the black bands of the scenes on or off (FUN_00417376,
    // DAT_00505b3c: 40 pixels at the top and the bottom)
    virtual void setCinemaBands(bool on) = 0;
    // 13: a voice of xa\ (FUN_00480e32), and whether it still plays
    virtual void playVoice(int voice) = 0;
    virtual bool voicePlaying() const = 0;
    // 16: the hero takes an item (FUN_00448833, then "You take...")
    virtual void giveItem(int item) = 0;
    // 20: a particle system thing of the level shown for some milliseconds
    // (FUN_0044f81d: the light of the other hero far away)
    virtual void showThing(int thing, int milliseconds) = 0;
    // 28: the flashlight on or off (FUN_00435751)
    virtual void setFlashlight(bool on) = 0;
    // 37: the hero at a marked place (FUN_00434a0e)
    virtual void putHeroAt(int marker) = 0;
};

class RadioScene {
public:
    // Starts a scene from its file (radio\N): its block 0 runs at once. The
    // numbered script to start when it is over (-1: none). False if it cannot
    // be read
    bool start(const std::string& path, int scriptAfter, RadioActions& actions);
    // Whether a scene runs
    bool running() const { return running_; }
    // One frame of the scene, milliseconds since the one before (FUN_00430dff).
    // When it is over, the numbered script to start (-1: none) goes to
    // scriptAfter and running() is false
    void step(float milliseconds, RadioActions& actions, int& scriptAfter);
    // Stops it at once (FUN_004311f2)
    void stop();
    // The orders the game has that the scene used and are not done
    const std::vector<int>& missingOrders() const { return missing_; }

private:
    struct Watcher {
        int order = -1;    // -1: free
        int block = 0;     // started when it is over
        size_t arguments = 0;
        float time = 0;    // milliseconds since it started
    };
    int32_t argument(size_t at, int index) const;
    void startBlock(int block, RadioActions& actions);
    void startOrder(int order, size_t arguments, RadioActions& actions);
    bool stillGoing(Watcher& w, RadioActions& actions);
    void noteMissing(int order);

    std::vector<uint8_t> data_;
    std::vector<uint32_t> blocks_;
    Watcher watchers_[16];
    bool running_ = false;
    int scriptAfter_ = -1;
    std::vector<int> missing_;
};
