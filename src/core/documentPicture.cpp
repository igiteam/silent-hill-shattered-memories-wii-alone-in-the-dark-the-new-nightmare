#include "documentPicture.h"

#include <cstring>
#include <filesystem>

#include "dataFile.h"

namespace {

constexpr size_t kHeader = 96;

uint16_t u16(const std::vector<uint8_t>& d, size_t at)
{
    return at + 2 <= d.size() ? uint16_t(d[at] | d[at + 1] << 8) : 0;
}

} // namespace

bool loadDocumentPicture(const std::string& dataFolder, const std::string& name, DocumentPicture& out)
{
    out = DocumentPicture();
    // The scripts' names may carry line breaks before them: only the
    // printable letters count
    std::string relative;
    for (char c : name)
        if (static_cast<unsigned char>(c) >= 0x20)
            relative += c == '\\' ? '/' : c;
    while (!relative.empty() && relative[0] == '/')
        relative.erase(0, 1);
    std::vector<uint8_t> d;
    if (!readDataFile((std::filesystem::path(dataFolder) / (relative + ".VRM")).string(), d) || d.size() < kHeader)
        return false;
    const std::string header(d.begin(), d.begin() + kHeader);
    if (header.find("DcBitmap") == std::string::npos)
        return false;
    const int sheetWidth = u16(d, 0x40), sheetHeight = u16(d, 0x42);
    const int width = u16(d, 0x46), height = u16(d, 0x48);
    if (width <= 0 || height <= 0 || width > sheetWidth || height > sheetHeight
        || d.size() < kHeader + size_t(sheetWidth) * size_t(sheetHeight) * 2)
        return false;
    out.width = width;
    out.height = height;
    out.rgba.resize(size_t(width) * size_t(height));
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            const uint16_t v = u16(d, kHeader + 2 * (size_t(y) * size_t(sheetWidth) + size_t(x)));
            const uint32_t r = (v >> 10 & 31) * 255 / 31, g = (v >> 5 & 31) * 255 / 31, b = (v & 31) * 255 / 31;
            out.rgba[size_t(y) * size_t(width) + size_t(x)] = 0xFF000000u | b << 16 | g << 8 | r;
        }
    return true;
}
