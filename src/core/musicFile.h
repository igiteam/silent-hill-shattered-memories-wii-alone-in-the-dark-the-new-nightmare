#pragma once

// The files of the music and the ambient sounds: midi\<hero>\<name> (MIDB,
// read by FUN_004982cf; docs/FORMATOS.md, "Sonido"). A bank number, tables
// of 128 bytes (a MIDI program -> an instrument of the file) and sequences
// with a name ("pluie_f", "act_b1"), each a DSEQ: its tempo, its speed, its
// marks (where its sections start and loop) and its MIDI events. The
// instruments themselves are samples of SOUND2 by tables of the executable
// (core/soundBanks); the block of instruments at the end of the file is not
// used by them.

#include <cstdint>
#include <string>
#include <vector>

struct MusicSequence {
    std::string name;
    std::vector<uint8_t> data;  // the DSEQ, from its "DSEQ"
    int table = 0;              // which program table of the file (+5)
    uint32_t speed = 0;         // +10, big end first: ticks a second x 1000
    std::vector<uint32_t> marks;  // +0x10, big end first: two a section (start, loop)
    size_t events = 0;          // where the events start (each one: its delta, then the event)
};

struct MusicFile {
    std::string name;           // as the game names it for its table of kinds ("act_b12.")
    int bank = 0;
    std::vector<std::vector<uint8_t>> tables;  // 128 bytes each
    std::vector<MusicSequence> sequences;
};

// Reads a file of midi\ by the name the scripts give ("\midi\carnby\jardind1").
// False if it is missing or not one
bool loadMusicFile(const std::string& dataFolder, const std::string& name, MusicFile& out);
