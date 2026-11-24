#include "viewPicture.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "dataFile.h"

namespace {

constexpr size_t kPixelsOffset = 0x3C;
constexpr size_t kTailSize = 2056; // two palettes of 256 BGRA colours and 8 bytes

uint16_t readU16(const uint8_t* p)
{
    return uint16_t(p[0] | (p[1] << 8));
}

// Steps through the values allowed by a bit mask, in order: the game stores
// the pixels in Z (Morton) order and builds the source offset of each row and
// column like this (FUN_004060c0 of alone4.exe)
std::vector<uint32_t> maskedSequence(int count, uint32_t step, uint32_t mask)
{
    std::vector<uint32_t> values(count);
    uint32_t v = 0;
    for (int i = 0; i < count; i++) {
        values[i] = v;
        v = (v + step) & mask;
    }
    return values;
}

} // namespace

bool loadViewSheet(const std::string& dataFolder, int level, int camera, bool lit, ViewSheet& out)
{
    char name[64];
    std::snprintf(name, sizeof(name), "view_%02d%c", camera, lit ? 'S' : 'D');
    std::filesystem::path path = std::filesystem::path(dataFolder) / "levels" / std::to_string(level) / name;
    std::vector<uint8_t> data;
    if (!readDataFile(path.string(), data) || data.size() < kPixelsOffset)
        return false;
    if (std::memcmp(data.data(), "LMVR", 4) != 0 && std::memcmp(data.data(), "VRAM", 4) != 0)
        return false;
    const uint32_t width = readU16(&data[0x0C]);
    const uint32_t height = readU16(&data[0x0E]);
    if (width < kViewWidth || height < kViewHeight || data.size() != kPixelsOffset + width * height + kTailSize)
        return false;

    uint32_t rowMask, colMask, rowStep, colStep;
    if (height < width) {
        rowMask = (width * width - 1) & 0x55555555u;
        colMask = (((width - height) * height) | 0xAAAAAAAAu) & (width * height - 1);
        rowStep = 0xAAAAAAABu;
        colStep = (height * height - 1) & 0x55555556u;
    } else {
        colMask = (height * height - 1) & 0xAAAAAAAAu;
        rowMask = (((height - width) * width) | 0x55555555u) & (width * height - 1);
        rowStep = (width * width - 1) & 0xAAAAAAABu;
        colStep = 0x55555556u;
    }
    const std::vector<uint32_t> rows = maskedSequence(int(height), rowStep, rowMask);
    const std::vector<uint32_t> cols = maskedSequence(int(width), colStep, colMask);

    const uint8_t* pixels = &data[kPixelsOffset];
    out.width = int(width);
    out.height = int(height);
    out.pixels.resize(size_t(width) * height);
    for (uint32_t y = 0; y < height; y++)
        for (uint32_t x = 0; x < width; x++)
            out.pixels[size_t(y) * width + x] = pixels[rows[y] | cols[x]];

    const uint8_t* palette = pixels + width * height; // 256 x BGRA (blue first); alpha unused
    for (int i = 0; i < 256; i++) {
        const uint8_t* c = palette + i * 4;
        out.palette[i] = uint32_t(c[2]) | (uint32_t(c[1]) << 8) | (uint32_t(c[0]) << 16) | 0xFF000000u;
    }
    return true;
}

bool loadViewPicture(const std::string& dataFolder, int level, int camera, bool lit, ViewPicture& out)
{
    ViewSheet sheet;
    if (!loadViewSheet(dataFolder, level, camera, lit, sheet))
        return false;
    out.rgba.resize(size_t(kViewWidth) * kViewHeight);
    for (int y = 0; y < kViewHeight; y++)
        for (int x = 0; x < kViewWidth; x++)
            out.rgba[size_t(y) * kViewWidth + x] = sheet.palette[sheet.pixels[size_t(y) * sheet.width + x]];
    return true;
}

std::vector<int> levelNumbers(const std::string& dataFolder)
{
    std::vector<int> levels;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(dataFolder) / "levels", error)) {
        const std::string name = entry.path().filename().string();
        if (entry.is_directory() && !name.empty() && std::all_of(name.begin(), name.end(), [](char c) { return std::isdigit((unsigned char)c) != 0; }))
            levels.push_back(std::stoi(name));
    }
    std::sort(levels.begin(), levels.end());
    return levels;
}

std::vector<int> cameraNumbers(const std::string& dataFolder, int level)
{
    std::vector<int> cameras;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(dataFolder) / "levels" / std::to_string(level), error)) {
        std::string name = entry.path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(), [](char c) { return char(std::tolower((unsigned char)c)); });
        if (name.size() == 8 && name.compare(0, 5, "view_") == 0 && name[7] == 's'
            && std::isdigit((unsigned char)name[5]) && std::isdigit((unsigned char)name[6]))
            cameras.push_back((name[5] - '0') * 10 + (name[6] - '0'));
    }
    std::sort(cameras.begin(), cameras.end());
    return cameras;
}
