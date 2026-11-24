#pragma once

// Saving and loading a game: our own file (UserData\<name>.sav, a text of
// "key value" lines), not the game's (_Alone4_*.sav, quick.sav: not studied).
// What is kept: the hero, where it is, its life, what it carries and holds,
// and what the scripts remember (the state of the game and their variables);
// the things of the level are set again by its script when it is loaded.

#include <cstdint>
#include <string>
#include <vector>

#include "levelScripts.h"

struct SaveGame {
    int hero = 0;          // 0 Carnby, 1 Aline
    int level = 1;
    int camera = -1;
    float place[3] = {};   // the feet, in the world
    float heading = 0;
    bool flashlightOn = true;
    int leftHand = 1, rightHand = 0;
    GameState state;
    std::vector<uint8_t> variables;  // the scripts' game variables
};

// Writes a game to UserData\<name>.sav. False if it could not be written
bool writeSaveGame(const std::string& name, const SaveGame& game);

// Reads a game from UserData\<name>.sav. False if there is none or it is not
// one of ours
bool readSaveGame(const std::string& name, SaveGame& out);
