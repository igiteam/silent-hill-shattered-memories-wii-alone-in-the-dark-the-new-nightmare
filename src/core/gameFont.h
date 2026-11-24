#pragma once

// The game's font8.pvr (docs/FORMATOS.md): a PVRT picture of 128 x 128 with
// the letters of ASCII 0x20 to 0x7F, 8 x 8 pixels each, each at the place of
// its code (16 to a row: the first two rows are empty). Only English letters:
// the accents of other languages are not in it.

#include <cstdint>
#include <string>
#include <vector>

constexpr int kGlyphSize = 8;

struct GameFont {
    int width = 0, height = 0;
    // White where a letter is drawn, see-through elsewhere (0xAABBGGRR)
    std::vector<uint32_t> rgba;
};

// Reads font8.pvr of the data folder. False if it is missing or not as expected
bool loadGameFont(const std::string& dataFolder, GameFont& out);

// Where a letter is in the font's picture (0..1); false for a letter it has
// not (it is then drawn as a space)
bool glyphPlace(const GameFont& font, unsigned char letter, float& u0, float& v0, float& u1, float& v1);
