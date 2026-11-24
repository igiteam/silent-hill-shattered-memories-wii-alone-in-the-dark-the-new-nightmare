#pragma once

// The hero's inventory, kept in the state of the game (the object the
// scripts get with function 22; FUN_00448833 and the ones after it): up to
// 0x53 items, each its number (the N of item\IT3D_N) and how many, in the
// order they were taken; the last one taken is the chosen one. And a list of
// numbers noted once (function 160, +0x150: what for is not known yet).

#include <vector>

#include "weaponTable.h"

struct InventoryItem {
    int item = 0;
    int count = 0;
};

struct Inventory {
    std::vector<InventoryItem> items;
    int chosen = -1;            // the item taken last (+2)
    std::vector<int> noted;     // function 160
};

// Takes "count" of an item (FUN_00448833) as the game: a box of a weapon's
// ammunition gives count x its shots per clip x its clips per box (the
// weapons of cweap.csv/aweap.csv); item 0x7E counts five each; item 0x7C is
// not kept: it adds 33 to item 0x5A, up to 100 (0x5A taken is 100); items
// 0x1A and 0x97 come with 4 the first time; others add count
void takeItem(Inventory& inventory, int item, int count, const std::vector<WeaponCharacteristics>& weapons);

// Drops an item altogether (FUN_00448a5a)
void dropItem(Inventory& inventory, int item);

// Whether it has an item (FUN_00448b24), and how many (FUN_00448b73; 0 if none)
bool hasItem(const Inventory& inventory, int item);
int itemCount(const Inventory& inventory, int item);

// Sets how many of an item it has, if it has it (FUN_00448bfb)
void setItemCount(Inventory& inventory, int item, int count);

// Notes a number once (FUN_00448d7e)
void noteNumber(Inventory& inventory, int number);
