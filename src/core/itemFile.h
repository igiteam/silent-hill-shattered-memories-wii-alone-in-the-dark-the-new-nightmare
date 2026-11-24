#pragma once

// The files of the items the hero can carry: item\IT3D_N (N: the item's
// number in the inventory). Chunk files like the menus' (core/menuFile): a
// DRAM with two texts (its description, and what the game says when it is
// taken: "You take a Small Steel Key.", or for several "...|You take %d Gas
// Cartridges."), a G3DB with its 3D model and an empty DR2D. The texts are
// the English ones; the Spanish version takes them from Provider\locale.

#include <string>

struct ItemInfo {
    std::string description;
    std::string taken;  // "You take a Small Steel Key." (its first part if it has two)
    std::string takenSeveral;  // "You take %d Gas Cartridges." ("" if none)
    std::string name;   // from what it says when taken: "Small Steel Key"
};

// Reads the texts of item N. False if it has no file
bool loadItemInfo(const std::string& dataFolder, int item, ItemInfo& out);

// What the game says when "count" of the item are taken
std::string takenText(const ItemInfo& info, int count);
