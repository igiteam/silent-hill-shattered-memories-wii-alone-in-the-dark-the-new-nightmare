#include "weaponTable.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <sstream>

#include "dataFile.h"

bool loadWeaponTable(const std::string& dataFolder, const std::string& file, std::vector<WeaponCharacteristics>& out)
{
    std::vector<uint8_t> data;
    if (!readDataFile((std::filesystem::path(dataFolder) / file).string(), data))
        return false;
    unscrambleTable(data);

    // Two lines of titles, then one per weapon starting with its number
    std::istringstream in(std::string(data.begin(), data.end()));
    std::string line;
    out.clear();
    while (std::getline(in, line)) {
        if (line.empty() || !std::isdigit(static_cast<unsigned char>(line[0])))
            continue;
        std::vector<std::string> cells;
        std::string cell;
        std::istringstream row(line);
        while (std::getline(row, cell, ';'))
            cells.push_back(cell);
        if (cells.size() < 12)
            return false;
        auto number = [&](size_t k) { return std::atoi(cells[k].c_str()); };
        WeaponCharacteristics w;
        w.number = number(0);
        w.name = cells[1];
        w.gauge = number(2) != 0;
        w.autolock = number(3) != 0;
        w.shotsPerClip = number(4);
        w.shotsPerBox = number(5);
        w.bulletsPerShot = number(6);
        w.damage = number(7);
        w.delay = number(8);
        w.item = number(9);
        w.ammunitionItem = number(10);
        w.impact = number(11);
        out.push_back(w);
    }
    return !out.empty();
}
