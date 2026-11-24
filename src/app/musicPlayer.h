#pragma once

// The music and the ambient sounds as the game plays them: the files of
// midi\ the scripts load (function 93), up to 5 sequences at once (94; the
// game's slots) stepped by its sequencer (core/musicSequencer) and their
// notes played with the instruments of the executable's tables
// (core/soundBanks) on at most 12 voices of the mixer (FUN_0049b5b6).

#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "musicFile.h"
#include "musicSequencer.h"
#include "soundBanks.h"

class MusicPlayer {
public:
    // The tables of instruments, and how to get a sample of SOUND2 by its
    // name (its number in the mixer, -1 if none); both kept by the caller
    void setup(const SoundBanks* banks, std::function<int(const std::string&)> sampleNamed);

    // Loads a file of midi\ ("\midi\carnby\jardind1"): it takes the place of
    // the one with the same bank. False if it cannot be read
    bool loadFile(const std::string& dataFolder, const std::string& name);

    // How many sequences can play at once (the channels of the state of the
    // game the scripts number, +0x1090)
    static constexpr int kSlots = 8;

    // Plays the sequence of that name on a slot (0..kSlots-1) with its volume left
    // and right (0..127), the section and how many times (0 for ever); if it
    // already plays there, only its volume changes. False if no loaded file
    // has it
    bool play(int slot, const std::string& sequence, int left, int right, int section, int repeats);
    void stop(int slot);
    void setVolume(int slot, int left, int right);
    // The section a slot plays and how many times (function 51, FUN_00499589;
    // a section it does not have is not taken)
    void setSection(int slot, int section, int repeats);
    void fade(int slot, int left, int right, int seconds);
    void stopAll();

    // Milliseconds of play since the last call
    void update(float milliseconds);

private:
    struct Voice {
        int voice = -1;   // of the mixer
        int slot = -1, channel = -1;
        float volume = 0;   // 0..1 it was started with
        float release = -1; // >= 0: fading out after its note off (milliseconds so far)
    };
    struct Slot {
        SequencePlayer player;
        int file = -1;
    };

    void apply(int slot, const MusicEvent& event);
    float voiceVolume(int slot, int channel) const;
    float voicePan(int slot, int channel) const;
    void endVoice(Voice& v, bool fadeOut);

    const SoundBanks* banks_ = nullptr;
    std::function<int(const std::string&)> sampleNamed_;
    std::deque<MusicFile> files_;  // (a deque: the players point into it)
    Slot slots_[kSlots];
    std::vector<Voice> voices_;
    float clock_ = 0;
};
