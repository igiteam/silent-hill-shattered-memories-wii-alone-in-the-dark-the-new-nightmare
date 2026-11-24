#include "handLight.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

#include "dataFile.h"

namespace {

uint32_t read32(const std::vector<uint8_t>& d, size_t at)
{
    return at + 4 <= d.size() ? uint32_t(d[at] | d[at + 1] << 8 | d[at + 2] << 16 | uint32_t(d[at + 3]) << 24) : 0;
}

uint16_t read16(const std::vector<uint8_t>& d, size_t at)
{
    return at + 2 <= d.size() ? uint16_t(d[at] | d[at + 1] << 8) : 0;
}

// Z order: the bits of x and y interleaved, y first
uint32_t twiddled(uint32_t x, uint32_t y)
{
    uint32_t r = 0;
    for (int bit = 0; bit < 11; bit++)
        r |= ((x >> bit) & 1u) << (2 * bit + 1) | ((y >> bit) & 1u) << (2 * bit);
    return r;
}

} // namespace

bool decodeBankTexture(const std::vector<uint8_t>& d, size_t block, size_t record, FlarePicture& out)
{
    const int width = read16(d, record), height = read16(d, record + 2);
    const uint32_t flags = read32(d, record + 4);
    const size_t pixels = block + read32(d, record + 0x28);
    const int side = std::min(width, height);
    if (side < 1 || width > 1024 || height > 1024 || pixels + size_t(width) * size_t(height) * 2 > d.size())
        return false;
    out.width = width;
    out.height = height;
    out.rgba.resize(size_t(width) * size_t(height));
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            const int square = width > height ? x / side : y / side;
            const size_t at = size_t(square) * size_t(side) * size_t(side) + twiddled(uint32_t(x % side), uint32_t(y % side));
            const uint32_t p = read16(d, pixels + 2 * at);
            uint32_t red, green, blue, alpha = 0xFF;
            if (flags & 1) {
                red = (p >> 11) & 31;
                green = ((p >> 5) & 63) * 31 / 63;
                blue = p & 31;
            } else {
                red = (p >> 10) & 31;
                green = (p >> 5) & 31;
                blue = p & 31;
                alpha = (p >> 15) ? 0xFF : 0;
            }
            out.rgba[size_t(y) * size_t(width) + size_t(x)] =
                (red * 255 / 31) | (green * 255 / 31) << 8 | (blue * 255 / 31) << 16 | alpha << 24;
        }
    return true;
}

namespace {

// The texture of the bank (from "bank", its G3DB chunk, on) with that name:
// its name 0x10 bytes before its record; the block starts 0x10 after the chunk
bool readNamedTexture(const std::vector<uint8_t>& d, size_t bank, const char* name, FlarePicture& out)
{
    const size_t length = std::strlen(name) + 1;
    const auto found = std::search(d.begin() + std::ptrdiff_t(bank), d.end(), name, name + length);
    if (found == d.end())
        return false;
    return decodeBankTexture(d, bank + 0x10, size_t(found - d.begin()) + 0x10, out);
}

// A colour of the flare (FUN_00417511): each part doubled, up to 255
uint32_t doubled(uint32_t colour)
{
    uint32_t out = 0;
    for (int part = 0; part < 3; part++) {
        const uint32_t c = (colour >> (8 * part)) & 0xFF;
        out |= (c < 0x80 ? c * 2 : 0xFF) << (8 * part);
    }
    return out;
}

} // namespace

bool loadNamedTexture(const std::string& dataFolder, const std::string& relativePath, const char* name,
                      FlarePicture& out)
{
    std::vector<uint8_t> d;
    if (!readDataFile((std::filesystem::path(dataFolder) / relativePath).string(), d))
        return false;
    // The chunks before the bank: tag, size, number of pointers, block, pointers
    size_t at = 0;
    while (at + 12 <= d.size() && std::memcmp(&d[at], "G3DB", 4) != 0)
        at += 12 + size_t(read32(d, at + 4)) + 4 * size_t(read32(d, at + 8));
    return at + 16 <= d.size() && readNamedTexture(d, at, name, out);
}

bool loadHandLight(const std::string& dataFolder, const std::string& relativePath, HandLight& out)
{
    out = HandLight();
    std::vector<uint8_t> d;
    if (!readDataFile((std::filesystem::path(dataFolder) / relativePath).string(), d) || d.size() < 0x24
        || std::memcmp(d.data(), "HAND", 4) != 0)
        return false;
    const size_t hand = 0x0C;
    out.flareColour = doubled(read32(d, hand + 4));
    out.spotColour = read32(d, hand + 8) & 0xFFFFFF;
    for (int k = 0; k < 3; k++)
        out.streakColour[k] = d[hand + 0x10 + size_t(k)];
    out.light = d[hand + 0x14] != 0;
    out.flags = d[hand + 0x15];
    const size_t bank = hand + read32(d, 4);
    if (out.light) {
        readNamedTexture(d, bank, "FLARE", out.flare);
        readNamedTexture(d, bank, "GLOW", out.glow);
        readNamedTexture(d, bank, "LINE", out.line);
    }
    return true;
}
