#pragma once

// The game's player of music sequences (FUN_004987f2, run by a Miles timer
// 200 times a second, FUN_004866ff). Each step it moves its clock by the
// sequence's speed / 200 and plays every event whose delta (x 1000) the clock
// has reached: notes (the game plays them an octave lower and one at a time
// a channel: a note off stops the channel's note), volume (control 7) and
// pan (10) of a channel, programs, the end (it stops), and the marks of its
// sections (control 98): at the end of the section chosen it goes back to
// where that section loops (a number of times, 0 for ever), and leaves for
// the section chosen elsewhere. Its own volume (left and right, 0..127 x
// 128) can move towards another one a step each time (function 95).

#include <cstdint>
#include <vector>

#include "musicFile.h"

// What a sequence asks of the sound: for channel 0..15
struct MusicEvent {
    enum Kind { NoteOn, NoteOff, Volume, Pan, Stop } kind = NoteOn;
    int channel = 0;
    int note = 60;     // NoteOn: MIDI note (already an octave down)
    int program = 0;   // NoteOn: the channel's MIDI program
};

struct SequencePlayer {
    const MusicSequence* sequence = nullptr;
    bool playing = false;
    size_t position = 0;          // of the next event's delta in its data
    int64_t clock = 0;            // FUN_004987f2's +0xBF0
    // Its volume, left and right (0..127 x 128), and where they move to
    // (-1: they stay) by how much a step
    int left = 127 << 7, right = 127 << 7;
    int leftTarget = -1, rightTarget = -1, leftStep = 0, rightStep = 0;
    int section = 0;              // the section chosen (+0xC20)
    int repeats = 0;              // how many more times it loops (0: for ever; +0xC22)
    int volume[16] = {}, pan[16] = {}, program[16] = {};
};

// The milliseconds of one step of the game's music timer (200 a second)
constexpr float kMusicStepMilliseconds = 5.0f;

// Starts a sequence from its beginning (FUN_004992f7): every channel at
// volume 127 and in the middle; its volume left and right (0..127), the
// section to play and how many times (0 for ever)
void startSequence(SequencePlayer& player, const MusicSequence& sequence, int left, int right, int section,
                   int repeats);

// Its volume now (0..127) and its pan (0 left .. 127 right), from left and
// right (FUN_004998b3)
int sequenceVolume(const SequencePlayer& player);
int sequencePan(const SequencePlayer& player);

// Moves its volume towards left and right (0..127) in that many seconds
// (FUN_00499b26)
void fadeSequence(SequencePlayer& player, int left, int right, int seconds);

// One step of the timer: the events played are added to out
void stepSequence(SequencePlayer& player, std::vector<MusicEvent>& out);
