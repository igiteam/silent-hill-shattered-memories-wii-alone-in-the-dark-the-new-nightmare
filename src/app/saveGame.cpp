#include "saveGame.h"

#include <sstream>

#include "userData.h"

namespace {

constexpr const char* kMagic = "NewNightmareSave";
constexpr int kVersion = 1;

std::string toHex(const uint8_t* bytes, size_t count)
{
    static const char digits[] = "0123456789abcdef";
    std::string s;
    for (size_t k = 0; k < count; k++) {
        s += digits[bytes[k] >> 4];
        s += digits[bytes[k] & 15];
    }
    return s.empty() ? "-" : s;
}

std::vector<uint8_t> fromHex(const std::string& s)
{
    std::vector<uint8_t> bytes;
    auto value = [](char c) { return c >= 'a' ? c - 'a' + 10 : c - '0'; };
    for (size_t k = 0; k + 1 < s.size(); k += 2)
        bytes.push_back(uint8_t(value(s[k]) << 4 | value(s[k + 1])));
    return bytes;
}

template <size_t N>
void copyHex(const std::string& s, uint8_t (&out)[N])
{
    const std::vector<uint8_t> bytes = fromHex(s);
    for (size_t k = 0; k < N && k < bytes.size(); k++)
        out[k] = bytes[k];
}

} // namespace

bool writeSaveGame(const std::string& name, const SaveGame& g)
{
    std::ostringstream out;
    out << kMagic << ' ' << kVersion << '\n';
    out << "hero " << g.hero << '\n' << "level " << g.level << '\n' << "camera " << g.camera << '\n';
    out << "place " << g.place[0] << ' ' << g.place[1] << ' ' << g.place[2] << '\n' << "heading " << g.heading << '\n';
    out << "flashlight " << (g.flashlightOn ? 1 : 0) << '\n';
    out << "hands " << g.leftHand << ' ' << g.rightHand << '\n';
    const GameState& s = g.state;
    out << "life " << s.life << '\n';
    out << "levels " << toHex(s.levels, sizeof s.levels) << '\n';
    out << "story " << toHex(s.story, sizeof s.story) << '\n';
    out << "done " << toHex(s.done, sizeof s.done) << '\n';
    out << "pair " << s.pair[0] << ' ' << s.pair[1] << '\n';
    out << "ranked " << s.ranked.size();
    for (const auto& r : s.ranked)
        out << ' ' << int(r.first) << ' ' << int(r.second);
    out << '\n' << "items " << s.inventory.items.size();
    for (const InventoryItem& i : s.inventory.items)
        out << ' ' << i.item << ' ' << i.count;
    out << '\n' << "chosen " << s.inventory.chosen << '\n' << "noted " << s.inventory.noted.size();
    for (int n : s.inventory.noted)
        out << ' ' << n;
    out << '\n' << "timers";
    for (const auto& bank : s.timers)
        for (const GameState::Timer& timer : bank)
            out << ' ' << timer.milliseconds << ' ' << timer.script;
    out << '\n' << "variables " << toHex(g.variables.data(), g.variables.size()) << '\n';
    return writeUserText(name + ".sav", out.str());
}

bool readSaveGame(const std::string& name, SaveGame& out)
{
    std::istringstream in(readUserText(name + ".sav"));
    std::string magic;
    int version = 0;
    if (!(in >> magic >> version) || magic != kMagic || version != kVersion)
        return false;
    out = SaveGame();
    GameState& s = out.state;
    std::string key;
    while (in >> key) {
        if (key == "hero")
            in >> out.hero;
        else if (key == "level")
            in >> out.level;
        else if (key == "camera")
            in >> out.camera;
        else if (key == "place")
            in >> out.place[0] >> out.place[1] >> out.place[2];
        else if (key == "heading")
            in >> out.heading;
        else if (key == "flashlight") {
            int on = 1;
            in >> on;
            out.flashlightOn = on != 0;
        } else if (key == "hands")
            in >> out.leftHand >> out.rightHand;
        else if (key == "life")
            in >> s.life;
        else if (key == "levels" || key == "story" || key == "done" || key == "variables") {
            std::string hex;
            in >> hex;
            if (key == "levels")
                copyHex(hex, s.levels);
            else if (key == "story")
                copyHex(hex, s.story);
            else if (key == "done")
                copyHex(hex, s.done);
            else
                out.variables = fromHex(hex);
        } else if (key == "pair")
            in >> s.pair[0] >> s.pair[1];
        else if (key == "ranked") {
            size_t n = 0;
            in >> n;
            for (size_t k = 0; k < n && k < 64; k++) {
                int id = 0, rank = 0;
                in >> id >> rank;
                s.ranked.push_back({ uint8_t(id), uint8_t(rank) });
            }
        } else if (key == "items") {
            size_t n = 0;
            in >> n;
            for (size_t k = 0; k < n && k < 256; k++) {
                InventoryItem item;
                in >> item.item >> item.count;
                s.inventory.items.push_back(item);
            }
        } else if (key == "timers") {
            for (auto& bank : s.timers)
                for (GameState::Timer& timer : bank)
                    in >> timer.milliseconds >> timer.script;
        } else if (key == "chosen")
            in >> s.inventory.chosen;
        else if (key == "noted") {
            size_t n = 0;
            in >> n;
            for (size_t k = 0; k < n && k < 256; k++) {
                int number = 0;
                in >> number;
                s.inventory.noted.push_back(number);
            }
        } else {
            std::string rest;
            std::getline(in, rest);  // a key of a later version: skipped
        }
    }
    return true;
}
