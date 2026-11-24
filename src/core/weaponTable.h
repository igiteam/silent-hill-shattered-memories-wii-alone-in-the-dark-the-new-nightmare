#pragma once

// The weapons of each hero (cweap.csv for Carnby, aweap.csv for Aline):
// scrambled tables like NpcCarac.csv, in French, one line per weapon: its
// number (the N of weapon\<hero>\weaponN), name, whether it has a gauge,
// whether it locks onto a target by itself, shots per clip and per box,
// bullets per shot, damage per shot, milliseconds between shots of the ones
// that fire on and on (-1: one a press), the item of the weapon and of its
// ammunition, and the kind of impact

#include <string>
#include <vector>

struct WeaponCharacteristics {
    int number = -1;
    std::string name;
    bool gauge = false;
    bool autolock = false;
    int shotsPerClip = 0, shotsPerBox = 0, bulletsPerShot = 0;
    int damage = 0;         // per shot
    int delay = -1;         // ms between shots, -1: one a press
    int item = -1, ammunitionItem = -1;
    int impact = 0;
};

// Reads the weapons of a hero (file: "cweap.csv" or "aweap.csv") of the data
// folder. False if it is missing or not as expected
bool loadWeaponTable(const std::string& dataFolder, const std::string& file, std::vector<WeaponCharacteristics>& out);
