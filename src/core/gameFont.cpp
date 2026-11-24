#include "gameFont.h"

#include <cstring>
#include <filesystem>

#include "dataFile.h"

namespace {

constexpr size_t kHeaderSize = 16;
constexpr unsigned char kFirstLetter = 0x20, kEndLetter = 0x80;

// A pixel's place in a square picture stored in Z order (y bits first), as
// the textures of the models (FUN_00420c64)
uint32_t twiddled(uint32_t x, uint32_t y)
{
    uint32_t out = 0;
    for (uint32_t bit = 0; bit < 16; bit++)
        out |= ((y >> bit) & 1u) << (2 * bit) | ((x >> bit) & 1u) << (2 * bit + 1);
    return out;
}

} // namespace

bool loadGameFont(const std::string& dataFolder, GameFont& out)
{
    out = GameFont{};
    std::vector<uint8_t> data;
    if (!readDataFile((std::filesystem::path(dataFolder) / "font8.pvr").string(), data) || data.size() < kHeaderSize
        || std::memcmp(data.data(), "PVRT", 4) != 0)
        return false;
    // Header: PVRT, size, pixel format (1: 5-6-5), layout (1: Z order), width, height
    const uint16_t width = uint16_t(data[12] | (data[13] << 8)), height = uint16_t(data[14] | (data[15] << 8));
    if (data[8] != 1 || data[9] != 1 || width != height || width == 0 || width > 1024
        || kHeaderSize + size_t(width) * height * 2 > data.size())
        return false;
    out.width = width;
    out.height = height;
    out.rgba.resize(size_t(width) * height);
    for (uint32_t y = 0; y < height; y++)
        for (uint32_t x = 0; x < width; x++) {
            const size_t at = kHeaderSize + size_t(twiddled(x, y)) * 2;
            const bool lit = (data[at] | data[at + 1]) != 0;  // letters are coloured, the rest black
            out.rgba[size_t(y) * width + x] = lit ? 0xFFFFFFFFu : 0x00000000u;
        }
    return true;
}

bool glyphPlace(const GameFont& font, unsigned char letter, float& u0, float& v0, float& u1, float& v1)
{
    if (font.width == 0 || letter < kFirstLetter || letter >= kEndLetter)
        return false;
    // Each letter at the place of its ASCII code: 16 to a row, so the space
    // starts the third row
    const int index = letter, perRow = font.width / kGlyphSize;
    const float column = float(index % perRow), row = float(index / perRow);
    u0 = column * kGlyphSize / float(font.width);
    v0 = row * kGlyphSize / float(font.height);
    u1 = u0 + float(kGlyphSize) / float(font.width);
    v1 = v0 + float(kGlyphSize) / float(font.height);
    return true;
}
