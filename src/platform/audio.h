#pragma once

// Sound: the game's samples (.wav, IMA ADPCM mono at 44100 Hz, read by SDL)
// played by a mixer of a few voices on SDL's default output, each with its
// volume, its pan and its pitch, as the game's sound system plays them
// (Miles Sound System: a sample per "program" of a MIDI channel)

#include <cstdint>
#include <string>

// Opens Windows' default output, at full volume; if the default changes
// while the game runs, the sound goes there. False if there is none (then
// nothing sounds)
bool startAudio();
void stopAudio();

// The output in use: always Windows' default ("none" if there is none)
std::string audioOutputName();

// A stream of sound made as it plays (a video's): stereo float samples at
// 44100 a second, given as they are decoded, through Windows' default output
// at full volume. How many of them are still waiting to be heard, to keep a
// picture in time with it
bool startStreamAudio();
void putStreamAudio(const float* stereo, int frames);
int streamAudioWaiting();
void stopStreamAudio();

// Loads a sample from a file; its number, or -1 if it cannot be read
int loadSound(const std::string& path);
// Keeps a sample already decoded (16-bit, mono, at that rate); its number, or
// -1 if it cannot
int loadSoundSamples(const int16_t* samples, int count, int rate);

// Plays a sample: volume 0..1, pan 0 (left) .. 1 (right), pitch 1 its own
// (2 an octave higher); looping ones go on until stopped. The number of the
// voice, or -1 if it cannot
int playSound(int sound, float volume, float pan, float pitch, bool loop = false);
// Whether a voice still plays
bool voicePlaying(int voice);
void setVoiceVolume(int voice, float volume, float pan);
void stopVoice(int voice);
void stopAllVoices();
