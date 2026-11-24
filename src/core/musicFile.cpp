#include "musicFile.h"

#include <cctype>
#include <cstring>
#include <filesystem>

#include "dataFile.h"

namespace {

uint32_t bigEnd(const std::vector<uint8_t>& d, size_t at, int bytes)
{
    uint32_t v = 0;
    for (int k = 0; k < bytes; k++)
        v = v << 8 | (at + size_t(k) < d.size() ? d[at + size_t(k)] : 0);
    return v;
}

} // namespace

bool loadMusicFile(const std::string& dataFolder, const std::string& name, MusicFile& out)
{
    out = MusicFile();
    std::string relative;
    for (char c : name)
        if (static_cast<unsigned char>(c) >= 0x20)
            relative += c == '\\' ? '/' : c;
    while (!relative.empty() && relative[0] == '/')
        relative.erase(0, 1);
    std::vector<uint8_t> d;
    if (!readDataFile((std::filesystem::path(dataFolder) / relative).string(), d) || d.size() < 7
        || std::memcmp(d.data(), "MIDB", 4) != 0)
        return false;
    // Its name for the table of kinds: the last part, lower case, with a dot
    const size_t slash = relative.find_last_of('/');
    out.name = relative.substr(slash == std::string::npos ? 0 : slash + 1);
    for (char& c : out.name)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    out.name += '.';
    out.bank = d[4];
    const int sequences = d[5], tables = d[6];
    size_t p = 7;
    for (int t = 0; t < tables && p + 128 <= d.size(); t++, p += 128)
        out.tables.emplace_back(d.begin() + long(p), d.begin() + long(p + 128));
    for (int s = 0; s < sequences && p + 12 <= d.size(); s++) {
        const size_t size = size_t(uint16_t(d[p] | d[p + 1] << 8));
        MusicSequence seq;
        for (size_t k = 0; k < 8 && d[p + 4 + k] != 0; k++)
            seq.name += char(d[p + 4 + k]);
        p += 12;
        if (p + size > d.size())
            return false;
        seq.data.assign(d.begin() + long(p), d.begin() + long(p + size));
        p += size;
        if (seq.data.size() < 16 || std::memcmp(seq.data.data(), "DSEQ", 4) != 0)
            continue;
        seq.table = seq.data[5];
        seq.speed = bigEnd(seq.data, 10, 4);
        const uint32_t marks = bigEnd(seq.data, 14, 2);
        for (uint32_t k = 0; k < marks; k++)
            seq.marks.push_back(bigEnd(seq.data, 16 + 4 * size_t(k), 4));
        seq.events = 16 + 4 * size_t(marks);
        out.sequences.push_back(std::move(seq));
    }
    return true;
}
