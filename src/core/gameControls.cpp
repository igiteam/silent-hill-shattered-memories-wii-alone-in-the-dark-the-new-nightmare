#include "gameControls.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>

#include "dataFile.h"

namespace {

constexpr size_t kEntrySize = 8;

} // namespace

GameControls defaultGameControls()
{
    return { 0xC8, 0xD0, 0xCB, 0xCD, 0x39, 0x1F, 0x1E, 0x17, 0x32, 0x13, 0x2A, 0x1D };
}

bool loadGameControls(const std::string& dataFolder, GameControls& out)
{
    out = defaultGameControls();
    std::vector<uint8_t> data;
    if (!readDataFile((std::filesystem::path(dataFolder) / "keys.bin").string(), data)
        || data.size() < kEntrySize * kGameActions)
        return false;
    for (size_t k = 0; k < kGameActions; k++) {
        uint32_t code;
        std::memcpy(&code, &data[kEntrySize * k], 4);
        if (code == 0 || code > 0xFF) {
            out = defaultGameControls();
            return false;
        }
        out[k] = int(code);
    }
    return true;
}
