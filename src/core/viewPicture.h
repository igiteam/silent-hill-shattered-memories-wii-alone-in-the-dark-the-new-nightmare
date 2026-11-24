#pragma once

// Backgrounds of the cameras: levels\<level>\view_NND (in the dark) and
// view_NNS (lit). LMVR format, explained in docs/FORMATOS.md.

#include <cstdint>
#include <string>
#include <vector>

constexpr int kViewWidth = 640;
constexpr int kViewHeight = 480;

struct ViewPicture {
    // kViewWidth x kViewHeight colours, 0xAABBGGRR (RGBA bytes in memory)
    std::vector<uint32_t> rgba;
};

// A whole LMVR sheet (1024 wide; the picture is its top left 640x480)
struct ViewSheet {
    int width = 0, height = 0;
    // width x height colour numbers, row after row. Bit 0x80 does not change
    // the colour: in the dark sheet it marks the background (characters pass
    // in front); without it the pixel belongs to an object of the foreground
    std::vector<uint8_t> pixels;
    // 256 colours 0xAABBGGRR (alpha 255); the last 128 repeat the first 128
    uint32_t palette[256] = {};
};

// Bit of a pixel of the dark sheet that marks the background
constexpr uint8_t kBackgroundBit = 0x80;

// Whole sheet of a background of a camera, lit (view_NNS) or in the dark
// (view_NND). False if the file is missing or is not an LMVR picture
bool loadViewSheet(const std::string& dataFolder, int level, int camera, bool lit, ViewSheet& out);

// Background of a camera of a level, lit or in the dark. False if the file is
// missing or is not an LMVR picture
bool loadViewPicture(const std::string& dataFolder, int level, int camera, bool lit, ViewPicture& out);

// Numbers of the levels of <dataFolder>\levels, in order
std::vector<int> levelNumbers(const std::string& dataFolder);

// Numbers of the cameras of a level (those with a view_NNS file), in order
std::vector<int> cameraNumbers(const std::string& dataFolder, int level);
