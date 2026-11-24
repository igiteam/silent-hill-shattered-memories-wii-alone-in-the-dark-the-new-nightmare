#pragma once

// The game's controls: keys.bin in the game's folder, written by its menu
// "configure controls" (docs/FORMATOS.md): 12 entries of 8 bytes, the
// DirectInput code of a key (u32) and 4 more bytes (0), in the order of the
// menu. Without the file, the game's own keys.

#include <array>
#include <string>

// The actions of the menu, in its order
enum GameAction {
    kActionUp,         // walk
    kActionDown,       // walk back; twice quickly: turn round
    kActionLeft,       // turn
    kActionRight,
    kActionUse,        // use / shoot: also answers questions and messages
    kActionFlashlight, // on and off
    kActionRun,
    kActionInventory,
    kActionMap,
    kActionRadio,
    kActionFreeLook,
    kActionAim,
    kGameActions
};

// The DirectInput code of the key of each action
using GameControls = std::array<int, kGameActions>;

// The game's own keys: arrows, Space, S, A, I, M, R, left Shift, left Ctrl
GameControls defaultGameControls();

// Reads keys.bin of the data folder; the game's own keys if it is missing or
// not as expected (then false)
bool loadGameControls(const std::string& dataFolder, GameControls& out);
