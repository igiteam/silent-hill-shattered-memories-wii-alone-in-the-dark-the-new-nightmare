#pragma once

// The light a left hand carries (lefthand\<hero>\handN, FUN_0044948d): the
// HAND chunk at the start of the file (its data at +0x0C, 0x18 bytes):
//   +0x04 the colour of the flare (FUN_00417511: each part doubled, up to 255)
//   +0x08 the colour of the spot light (FUN_004184cd)
//   +0x10 the colour of the streak (three bytes, FUN_00419312)
//   +0x14 whether it is a light (FUN_00449791; without it, no flare nor spot)
//   +0x15 flags: 1 narrow beam (FUN_00418689), 2 long reach (0x4E2, else 0x271)
// and the pictures of its flare in the hand's G3DB bank, found by name by the
// game (FUN_00422340: "FLARE", "GLOW", "LINE"): each a texture record (0x30
// bytes, its name 0x10 bytes before it) whose pixels are 0x10 bytes further
// than the place it gives, 16 bits in Z order (rectangles: squares of the
// small side one after the other), RGB 5-6-5 with flag 1.
// Colours: their bytes are red, green, blue (FUN_00414fa9 puts the first in
// the red of its vertices).

#include <cstdint>
#include <string>
#include <vector>

struct FlarePicture {
    int width = 0, height = 0;
    std::vector<uint32_t> rgba;  // 0xAABBGGRR
};

struct HandLight {
    bool light = false;
    uint8_t flags = 0;
    uint32_t flareColour = 0, spotColour = 0;  // 0x00BBGGRR
    uint8_t streakColour[3] = {};  // red, green, blue
    FlarePicture flare, glow, line;
};

// Decodes a texture record (0x30 bytes at "record" of a G3DB block: width,
// height, flags, and at +0x28 where its pixels are in the block): 16 bits a
// pixel in Z order (rectangles: squares of the small side one after the
// other), RGB 5-6-5 with flag 1, else 1-5-5-5 (the top bit opaque). False if
// it is not one
bool decodeBankTexture(const std::vector<uint8_t>& data, size_t block, size_t record, FlarePicture& out);

// A texture of a file's G3DB bank found by its name (the flare's "FLARE",
// a weapon's "BALLES": the bullets of the aiming screen). False if none
bool loadNamedTexture(const std::string& dataFolder, const std::string& relativePath, const char* name,
                      FlarePicture& out);

// Reads the HAND chunk and the flare's pictures of a file of lefthand\
// ("lefthand\\carnby\\hand1"). False if it cannot be read
bool loadHandLight(const std::string& dataFolder, const std::string& relativePath, HandLight& out);
