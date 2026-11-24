#pragma once

// The sound banks of the game. Each data file that brings sounds (the files
// of the levels, levels\N\CarnbyNN; the books...) has a DSND chunk with the
// number of its bank (0 for a level: FUN_0049a1ea), and the game fills the
// bank's slots with samples of SOUND2 by a table in its executable
// (FUN_0049bc7b): for each file name ("levels\1\carnby01.") a list of
// (group, item); 42 groups of samples ("porte", "hybride", "c_marche"...)
// with an entry of 0x14 bytes per sample (its name, "porte\0001_b", and
// three values). The script functions play the slots (function 45).
//
// The tables are read from the player's own alone4.exe (English and Spanish
// ones differ in their addresses): they are found by their contents, the
// name of the first group ("a_action") and of the first file
// ("book\docplate."), and the pointers to them.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// An instrument of the music (FUN_0049c5ae): a sample of SOUND2 played at
// the pitch of each note, 44100 x the note's frequency / its root's
// (FUN_0049b5b6); looping ones repeat until the note is let go
struct MusicInstrument {
    std::string sample;     // name in SOUND2 ("" none)
    bool loops = false;     // its flag is 3
    float rootFrequency = 0;  // Hz x 100 of the note the sample is (26163: middle C)
};

struct SoundBanks {
    // For each file (lower case, as the game writes it: "levels\1\carnby01."),
    // the samples of its slots (names of SOUND2 without ".wav"; empty: none)
    std::map<std::string, std::vector<std::string>> files;
    // The root of each of those samples (Hz x 100 of the note the sample is,
    // +0x0C of its entry; 0: not given): a slot played as a note sounds at
    // 44100 x the note's frequency / its root (FUN_0049b5b6)
    std::map<std::string, std::vector<float>> roots;
    // The music's instruments by MIDI program: the musical ones (bass01c,
    // drums01c...) and the ambient ones (6004_a, avion01...), two tables of
    // 128 entries of 0x14 bytes (flag, name, a value, root x 100); and for
    // each file of midi\ ("act_b12.") which table each program takes (2:
    // the ambient one; FUN_0049b090)
    MusicInstrument musical[128], ambient[128];
    std::map<std::string, std::vector<uint8_t>> musicKinds;
};

// Reads the tables from alone4.exe of the data folder. False if they cannot
// be found
bool loadSoundBanks(const std::string& dataFolder, SoundBanks& out);

// The name of the bank of a level's file ("levels\<level>\<file>.", lower case)
std::string levelSoundBankName(int level, const std::string& file);
