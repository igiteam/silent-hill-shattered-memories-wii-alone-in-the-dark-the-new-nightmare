#include "dataFile.h"

#include <cstdio>

#include <zlib.h>

namespace {

bool readRaw(const std::string& path, std::vector<uint8_t>& out)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return false;
    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    out.resize(size > 0 ? size_t(size) : 0);
    size_t read = out.empty() ? 0 : std::fread(out.data(), 1, out.size(), file);
    std::fclose(file);
    return read == out.size();
}

// Unpacks one gzip stream; the zeros after it (CD sector padding) are ignored
bool gunzip(const std::vector<uint8_t>& packed, std::vector<uint8_t>& out)
{
    z_stream stream = {};
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK)
        return false;
    stream.next_in = const_cast<Bytef*>(packed.data());
    stream.avail_in = uInt(packed.size());
    out.clear();
    uint8_t chunk[65536];
    int result;
    do {
        stream.next_out = chunk;
        stream.avail_out = sizeof(chunk);
        result = inflate(&stream, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END) {
            inflateEnd(&stream);
            return false;
        }
        out.insert(out.end(), chunk, chunk + (sizeof(chunk) - stream.avail_out));
    } while (result != Z_STREAM_END);
    inflateEnd(&stream);
    return true;
}

} // namespace

bool readDataFile(const std::string& path, std::vector<uint8_t>& out)
{
    std::vector<uint8_t> raw;
    if (!readRaw(path, raw))
        return false;
    if (raw.size() >= 2 && raw[0] == 0x1F && raw[1] == 0x8B)
        return gunzip(raw, out);
    out.swap(raw);
    return true;
}

void unscrambleTable(std::vector<uint8_t>& data)
{
    for (size_t i = 0; i < data.size(); i++) {
        const uint8_t x = uint8_t(data[i] ^ uint8_t(i));
        data[i] = uint8_t((x & 0x3C) | ((x & 0x03) << 6) | (x >> 6));
    }
}
