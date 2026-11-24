#pragma once

// The files of the game's menus: menu\menu2 (title and options), select2
// (choosing Carnby or Aline), ingame2 (pause). Gzip chunk files like the
// levels' (docs/FORMATOS.md, "Menús"):
//
// - DR2D (FUN_0048875a): the pictures. A count, then each one aligned to 32
//   bytes: its name (32 bytes), width, height, a kind (u16, 0 in every
//   file), the width and height it uses of it, and aligned to 32 its pixels
//   (16 bits, 1-5-5-5: the top bit is opaque).
// - DRAM: the menu's pages (not studied yet) with, at +0x12 of its header,
//   how many fonts and at +0x48 where (0x28 bytes each, FUN_0046de3b): where
//   its letters are, its picture's name. The letters ("SPC\0", a count, 0,
//   then 8 bytes a letter: top, right end of its ink, height, where the next
//   one starts) go in the order of the Latin-1 codes they draw; and at +0x50
//   the menu's texts (pointers to English texts: "New Game"...).

#include <cstdint>
#include <string>
#include <vector>

// A picture of a menu, as colours 0xAABBGGRR (alpha 0: a hole)
struct MenuPicture {
    std::string name;
    int width = 0, height = 0;  // of the pixels
    int usedWidth = 0, usedHeight = 0;  // what is drawn of them, from the top left
    std::vector<uint32_t> rgba;
};

// Where a letter of a menu font is on its picture (pixels)
struct MenuGlyph {
    int left = 0, top = 0, right = 0, bottom = 0;
    int rowTop = 0;  // the highest top of its row: the letters line up by it
};

// A font of a menu: its picture's name and its letters by Latin-1 code
// (right == left: none)
struct MenuFont {
    std::string picture;
    MenuGlyph glyphs[256];
    int height = 0;  // of its line
};

struct MenuFile {
    std::vector<MenuPicture> pictures;
    std::vector<MenuFont> fonts;
    std::vector<std::string> texts;
};

// Reads a menu file (relative to the data folder: "menu\\menu2"). False if
// it is missing or not as expected
bool loadMenuFile(const std::string& dataFolder, const std::string& relativePath, MenuFile& out);

// A picture of the menu by its name (nullptr: none)
const MenuPicture* menuPicture(const MenuFile& menu, const std::string& name);
