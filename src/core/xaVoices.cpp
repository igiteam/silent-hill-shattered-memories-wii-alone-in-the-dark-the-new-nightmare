#include "xaVoices.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "dataFile.h"

namespace {

uint32_t bigEndian32(const uint8_t* p)
{
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}

uint16_t bigEndian16(const uint8_t* p)
{
    return uint16_t(p[0] << 8 | p[1]);
}

uint32_t littleEndian32(const uint8_t* p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

} // namespace

bool XaVoices::load(const std::string& dataFolder)
{
    folder_ = (std::filesystem::path(dataFolder) / "xa").string();
    places_.clear();
    std::vector<uint8_t> data;
    if (!readDataFile((std::filesystem::path(dataFolder) / "Xa.txt").string(), data))
        return false;
    unscrambleTable(data);
    std::istringstream in(std::string(data.begin(), data.end()));
    std::string line;
    while (std::getline(in, line)) {
        const size_t comma = line.find(',');
        if (comma == std::string::npos)
            continue;
        places_.push_back({ std::atoi(line.c_str()), std::atoi(line.c_str() + comma + 1) });
    }
    return !places_.empty();
}

bool XaVoices::read(int voice, std::vector<int16_t>& samples, int& rate) const
{
    if (voice < 0 || voice >= count())
        return false;
    char name[16];
    std::snprintf(name, sizeof(name), "Xa%02d.xa", places_[size_t(voice)].first);
    std::ifstream file(std::filesystem::path(folder_) / name, std::ios::binary);
    if (!file)
        return false;
    uint8_t header[8];
    if (!file.read(reinterpret_cast<char*>(header), 8) || header[0] != 'A' || header[1] != 'F' || header[2] != 'S')
        return false;
    const int entry = places_[size_t(voice)].second;
    if (entry < 0 || uint32_t(entry) >= littleEndian32(header + 4))
        return false;
    uint8_t place[8];
    file.seekg(8 + 8 * entry);
    if (!file.read(reinterpret_cast<char*>(place), 8))
        return false;
    std::vector<uint8_t> sound(littleEndian32(place + 4));
    file.seekg(littleEndian32(place));
    if (sound.empty() || !file.read(reinterpret_cast<char*>(sound.data()), std::streamsize(sound.size())))
        return false;
    return decodeAdx(sound.data(), sound.size(), samples, rate);
}

bool decodeAdx(const uint8_t* data, size_t size, std::vector<int16_t>& samples, int& rate)
{
    if (size < 0x14 || data[0] != 0x80 || data[1] != 0x00)
        return false;
    const size_t start = size_t(bigEndian16(data + 2)) + 4;  // after "(c)CRI"
    const int frameSize = data[5], bits = data[6], channels = std::max<int>(data[7], 1);
    rate = int(bigEndian32(data + 8));
    const uint32_t total = bigEndian32(data + 12);
    const int cutoff = bigEndian16(data + 16);
    if (data[4] != 3 || bits != 4 || frameSize < 3 || rate <= 0 || start >= size)
        return false;
    // The prediction from the high-pass frequency, in 1/4096
    const double a = std::sqrt(2.0) - std::cos(2.0 * 3.14159265358979 * cutoff / rate);
    const double b = std::sqrt(2.0) - 1.0;
    const double c = (a - std::sqrt((a + b) * (a - b))) / b;
    const int coefficient1 = int(std::lround(c * 2.0 * 4096.0));
    const int coefficient2 = int(std::lround(-(c * c) * 4096.0));
    const int perFrame = (frameSize - 2) * 2;
    samples.clear();
    samples.reserve(total);
    int before1 = 0, before2 = 0;
    for (size_t at = start; at + size_t(frameSize) * size_t(channels) <= size && samples.size() < total;
         at += size_t(frameSize) * size_t(channels)) {
        const int scale = bigEndian16(data + at);
        if (scale & 0x8000)  // the end mark
            break;
        for (int k = 0; k < perFrame && samples.size() < total; k++) {
            const uint8_t byte = data[at + 2 + size_t(k / 2)];
            int nibble = (k & 1) ? (byte & 0x0F) : (byte >> 4);
            if (nibble & 8)
                nibble -= 16;
            int sample = nibble * scale + ((coefficient1 * before1 + coefficient2 * before2) >> 12);
            sample = std::clamp(sample, -32768, 32767);
            before2 = before1;
            before1 = sample;
            samples.push_back(int16_t(sample));
        }
    }
    return !samples.empty();
}
