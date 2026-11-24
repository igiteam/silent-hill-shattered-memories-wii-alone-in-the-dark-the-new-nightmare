#include "soundBanks.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>

#include "dataFile.h"

namespace {

// The executable as it is in memory: its sections, to turn addresses into
// places of the file
class Executable {
public:
    explicit Executable(const std::vector<uint8_t>& d) : d_(d)
    {
        if (d.size() < 0x40)
            return;
        const uint32_t pe = u32(0x3C);
        if (size_t(pe) + 0x18 > d.size() || std::memcmp(d.data() + pe, "PE\0\0", 4) != 0)
            return;
        const uint16_t sections = u16(pe + 6), optional = u16(pe + 20);
        base_ = u32(pe + 24 + 28);
        for (uint16_t k = 0; k < sections; k++) {
            const size_t s = size_t(pe) + 24 + optional + 40 * size_t(k);
            if (s + 40 > d.size())
                return;
            sections_.push_back({ u32(s + 12), std::max(u32(s + 8), u32(s + 16)), u32(s + 20), u32(s + 16) });
        }
        good_ = true;
    }

    bool good() const { return good_; }
    uint32_t u32(size_t at) const
    {
        uint32_t v = 0;
        if (at + 4 <= d_.size())
            std::memcpy(&v, d_.data() + at, 4);
        return v;
    }
    uint16_t u16(size_t at) const
    {
        uint16_t v = 0;
        if (at + 2 <= d_.size())
            std::memcpy(&v, d_.data() + at, 2);
        return v;
    }
    // The place in the file of an address, or 0 if it is not in the file
    size_t place(uint32_t address) const
    {
        const uint32_t rva = address - base_;
        for (const Section& s : sections_)
            if (rva >= s.address && rva < s.address + s.size && rva - s.address < s.rawSize)
                return s.raw + (rva - s.address);
        return 0;
    }
    uint32_t address(size_t place) const
    {
        for (const Section& s : sections_)
            if (place >= s.raw && place < size_t(s.raw) + s.rawSize)
                return base_ + s.address + uint32_t(place - s.raw);
        return 0;
    }
    // The text at an address ("" if none)
    std::string text(uint32_t address) const
    {
        const size_t at = place(address);
        if (at == 0)
            return {};
        std::string s;
        for (size_t k = at; k < d_.size() && d_[k] != 0 && s.size() < 256; k++)
            s += char(d_[k]);
        return s;
    }
    // Where a text is (as a whole, with its zero), or 0
    size_t findText(const char* text) const
    {
        const size_t n = std::strlen(text) + 1;
        auto it = std::search(d_.begin(), d_.end(), text, text + n);
        while (it != d_.end()) {
            const size_t at = size_t(it - d_.begin());
            if (at == 0 || d_[at - 1] == 0)
                return at;
            it = std::search(it + 1, d_.end(), text, text + n);
        }
        return 0;
    }
    // The first place (4-byte aligned) holding a value, or 0
    size_t findValue(uint32_t value, size_t from = 0) const
    {
        for (size_t at = from & ~size_t(3); at + 4 <= d_.size(); at += 4)
            if (u32(at) == value)
                return at;
        return 0;
    }

private:
    struct Section {
        uint32_t address, size, raw, rawSize;
    };
    const std::vector<uint8_t>& d_;
    std::vector<Section> sections_;
    uint32_t base_ = 0;
    bool good_ = false;
};

std::string lower(std::string s)
{
    for (char& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// The music's tables (found by their contents: the musical one points to
// "bass01c" at its program 0, the ambient one to "avion01" at its program
// 70, the list of files to "act_b12." first)
void readMusicTables(const Executable& exe, SoundBanks& out)
{
    auto readTable = [&](size_t at, MusicInstrument (&table)[128]) {
        for (size_t k = 0; k < 128; k++) {
            const size_t e = at + 0x14 * k;
            MusicInstrument& i = table[k];
            i.loops = exe.u32(e) == 3;
            const uint32_t name = exe.u32(e + 4);
            i.sample = name ? exe.text(name) : "";
            const uint32_t root = exe.u32(e + 12);
            std::memcpy(&i.rootFrequency, &root, 4);
        }
    };
    const size_t bass = exe.findText("bass01c");
    const size_t musical = bass ? exe.findValue(exe.address(bass)) : 0;
    if (musical >= 4)
        readTable(musical - 4, out.musical);
    const size_t avion = exe.findText("avion01");
    const size_t ambient = avion ? exe.findValue(exe.address(avion)) : 0;
    if (ambient >= 4 + 70 * 0x14)
        readTable(ambient - 4 - 70 * 0x14, out.ambient);
    const size_t first = exe.findText("act_b12.");
    const size_t list = first ? exe.findValue(exe.address(first)) : 0;
    for (size_t at = list; list && out.musicKinds.size() < 256; at += 8) {
        const std::string name = exe.text(exe.u32(at));
        const size_t kinds = exe.place(exe.u32(at + 4));
        if (name.empty() || kinds == 0)
            break;
        std::vector<uint8_t>& k = out.musicKinds[lower(name)];
        for (size_t p = 0; p < 128; p++)
            k.push_back(uint8_t(exe.u32(kinds + p) & 0xFF));
    }
}

} // namespace

std::string levelSoundBankName(int level, const std::string& file)
{
    return lower("levels\\" + std::to_string(level) + "\\" + file + ".");
}

bool loadSoundBanks(const std::string& dataFolder, SoundBanks& out)
{
    std::vector<uint8_t> data;
    if (!readDataFile((std::filesystem::path(dataFolder) / "alone4.exe").string(), data))
        return false;
    const Executable exe(data);
    if (!exe.good())
        return false;

    // The groups: the header of the first one points to its name, a_action
    // (header: 0, name, 0, a float, 0); the table of groups points to it
    const size_t groupName = exe.findText("a_action");
    if (groupName == 0)
        return false;
    const size_t namePointer = exe.findValue(exe.address(groupName));
    if (namePointer < 4)
        return false;
    const size_t groupTable = exe.findValue(exe.address(namePointer - 4));
    if (groupTable == 0)
        return false;
    std::vector<uint32_t> groups;
    for (size_t at = groupTable; groups.size() < 256; at += 4) {
        const uint32_t g = exe.u32(at);
        if (g == 0 || exe.place(g) == 0)
            break;
        groups.push_back(g);
    }

    // The files: the first one is book\docplate. (name, list of
    // (group, item) ending with -1)
    const size_t fileName = exe.findText("book\\docplate.");
    if (fileName == 0)
        return false;
    const size_t fileTable = exe.findValue(exe.address(fileName));
    if (fileTable == 0)
        return false;
    for (size_t at = fileTable; out.files.size() < 4096; at += 8) {
        const std::string name = exe.text(exe.u32(at));
        const size_t list = exe.place(exe.u32(at + 4));
        if (name.empty() || list == 0)
            break;
        std::vector<std::string>& slots = out.files[lower(name)];
        std::vector<float>& roots = out.roots[lower(name)];
        for (size_t p = list; slots.size() < 512; p += 8) {
            const int32_t group = int32_t(exe.u32(p)), item = int32_t(exe.u32(p + 4));
            if (group == -1 || item == -1)
                break;
            std::string sample;
            float root = 0;
            if (group >= 0 && size_t(group) < groups.size()) {
                // The item's entry, after the group's header (0x14 bytes each)
                const size_t entry = exe.place(groups[size_t(group)]) + 0x14 * (size_t(item) + 1);
                sample = exe.text(exe.u32(entry + 4));
                const uint32_t bits = exe.u32(entry + 12);
                std::memcpy(&root, &bits, 4);
                const size_t slash = sample.find_last_of("\\/");
                if (slash != std::string::npos)
                    sample = sample.substr(slash + 1);  // SOUND2 has no folders
            }
            slots.push_back(sample);
            roots.push_back(root);
        }
    }
    readMusicTables(exe, out);
    return !out.files.empty();
}
