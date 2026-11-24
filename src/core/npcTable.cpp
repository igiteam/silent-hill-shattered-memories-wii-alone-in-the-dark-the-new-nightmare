#include "npcTable.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <vector>

#include "dataFile.h"

namespace {

// The cells of a line, split at each ';'
std::vector<std::string> cellsOf(const std::string& line)
{
    std::vector<std::string> cells;
    std::string cell;
    std::istringstream in(line);
    while (std::getline(in, cell, ';'))
        cells.push_back(cell);
    return cells;
}

bool blank(const std::string& s)
{
    return s.find_first_not_of(" \t\r") == std::string::npos;
}

} // namespace

bool loadNpcTable(const std::string& dataFolder, std::array<NpcCharacteristics, kNpcKinds>& out)
{
    std::vector<uint8_t> data;
    if (!readDataFile((std::filesystem::path(dataFolder) / "NpcCarac.csv").string(), data))
        return false;
    unscrambleTable(data);

    // The lines: a header, then per monster a line with its name and kind
    // and 8 + 24 lines with a value in the fifth cell
    std::vector<std::vector<std::string>> lines;
    std::string line;
    std::istringstream in(std::string(data.begin(), data.end()));
    while (std::getline(in, line)) {
        if (blank(line))
            continue;
        lines.push_back(cellsOf(line));
    }
    bool good = true;
    for (size_t k = 1; k < lines.size();) {
        const std::vector<std::string>& head = lines[k];
        if (head.size() < 2 || blank(head[0])) {
            good = false;
            k++;
            continue;
        }
        const int kind = std::atoi(head[1].c_str());
        size_t n = 0;
        NpcCharacteristics c;
        c.name = head[0];
        while (c.name.size() && c.name.back() == ' ')
            c.name.pop_back();
        for (k++; k < lines.size() && n < c.values.size(); k++) {
            const std::vector<std::string>& row = lines[k];
            if (!row.empty() && !blank(row[0]))
                break;  // the next monster
            if (row.size() < 5) {
                good = false;
                continue;
            }
            c.values[n++] = int16_t(std::atoi(row[4].c_str()));
        }
        if (n != c.values.size())
            good = false;
        if (kind >= 0 && kind < kNpcKinds)
            out[size_t(kind)] = c;
        else
            good = false;
    }
    return good;
}
