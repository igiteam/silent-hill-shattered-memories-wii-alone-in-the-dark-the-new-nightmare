#pragma once

// The characteristics of the monsters (NpcCarac.csv, FUN_00455080): a table
// in French, scrambled byte by byte (FUN_00479cd2), with one block per monster
// (OPHTALMICIDE, PHOTOSAURE, ARRACHEUR...): its name and kind (0-15), 8 values
// (weight, life points, damage of its first and second attacks, damage per
// cycle while it holds the hero, how many cycles it holds, presses of the
// action button to get free, damage it takes meanwhile) and 12 weapons with 2
// values each (how far a shot pushes it back). The game keeps them as 32
// numbers per kind (at 0x507B60, 0x40 bytes each)

#include <array>
#include <cstdint>
#include <string>

constexpr int kNpcKinds = 16;

struct NpcCharacteristics {
    std::string name;  // empty: no block for the kind
    std::array<int16_t, 32> values = {};

    int weight() const { return values[0]; }
    int life() const { return values[1]; }
    int firstAttackDamage() const { return values[2]; }
    int secondAttackDamage() const { return values[3]; }
    int holdDamage() const { return values[4]; }
    int holdCycles() const { return values[5]; }
    int pressesToGetFree() const { return values[6]; }
    int damageWhileHolding() const { return values[7]; }  // +0xE: taken per hit (FUN_00452096)
};

// Reads NpcCarac.csv of the data folder. False if it is missing or not as
// expected (the kinds read stay filled)
bool loadNpcTable(const std::string& dataFolder, std::array<NpcCharacteristics, kNpcKinds>& out);
