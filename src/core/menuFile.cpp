#include "menuFile.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

#include "dataFile.h"

namespace {

uint16_t u16(const std::vector<uint8_t>& d, size_t at)
{
    return at + 2 <= d.size() ? uint16_t(d[at] | d[at + 1] << 8) : 0;
}

uint32_t u32(const std::vector<uint8_t>& d, size_t at)
{
    return at + 4 <= d.size() ? uint32_t(u16(d, at) | uint32_t(u16(d, at + 2)) << 16) : 0;
}

size_t align32(size_t at)
{
    return (at + 31) & ~size_t(31);
}

std::string textAt(const std::vector<uint8_t>& d, size_t at, size_t most)
{
    std::string s;
    for (size_t k = at; k < d.size() && k < at + most && d[k] != 0; k++)
        s += char(d[k]);
    return s;
}

// The Latin-1 codes of the letters of the menu fonts, in the order they are
// drawn on their pictures: the 161 both fonts share (from '!' up, the
// letters with accents of Western Europe), then a few of their own
// (BMP_COURIER7: ¡ © ®, an arrow and the buttons of a pad, °; BMP_OLDBOOK:
// three like ¡, © ® ÿ)
std::vector<int> fontCodes(size_t count)
{
    std::vector<int> codes;
    for (int c = 0x21; c <= 0x7E; c++)
        codes.push_back(c);
    for (int c : { 0x88, 0x8A, 0x8B, 0x98, 0x9A, 0x9B, 0x9F, 0xAB, 0xBB, 0xBF })
        codes.push_back(c);
    const int ranges[][2] = { { 0xC0, 0xC5 }, { 0xC7, 0xD6 }, { 0xD8, 0xDD }, { 0xDF, 0xE5 },
                              { 0xE7, 0xEF }, { 0xF1, 0xF6 }, { 0xF8, 0xFD } };
    for (const auto& r : ranges)
        for (int c = r[0]; c <= r[1]; c++)
            codes.push_back(c);
    if (count == 170) {
        codes.push_back(0xFF);
        for (int c : { 0xA1, 0xA9, 0xAE })
            codes.push_back(c);
    } else
        for (int c : { 0xA1, -1, -1, 0xA9, 0xAE, 0xFF })
            codes.push_back(c);
    codes.resize(count, -1);
    return codes;
}

// The DR2D chunk at "at" (its letters): the pictures
bool readPictures(const std::vector<uint8_t>& d, size_t at, std::vector<MenuPicture>& out)
{
    const size_t size = u32(d, at + 4);
    const size_t start = at + 8;
    if (start + size > d.size())
        return false;
    const uint32_t count = u32(d, start);
    size_t p = 4;
    for (uint32_t k = 0; k < count; k++) {
        p = align32(p);
        if (p + 0x2A > size)
            return false;
        MenuPicture picture;
        picture.name = textAt(d, start + p, 32);
        picture.width = u16(d, start + p + 0x20);
        picture.height = u16(d, start + p + 0x22);
        picture.usedWidth = u16(d, start + p + 0x26);
        picture.usedHeight = u16(d, start + p + 0x28);
        p = align32(p + 0x2A);
        const size_t pixels = size_t(picture.width) * size_t(picture.height);
        if (p + 2 * pixels > size)
            return false;
        picture.rgba.resize(pixels);
        for (size_t i = 0; i < pixels; i++) {
            const uint16_t v = u16(d, start + p + 2 * i);
            const uint32_t r = (v >> 10 & 31) * 255 / 31, g = (v >> 5 & 31) * 255 / 31, b = (v & 31) * 255 / 31;
            picture.rgba[i] = ((v & 0x8000) ? 0xFF000000u : 0u) | b << 16 | g << 8 | r;
        }
        p += 2 * pixels;
        out.push_back(std::move(picture));
    }
    return true;
}

// The fonts and texts of the DRAM block (block: where it starts, size)
void readFontsAndTexts(const std::vector<uint8_t>& d, size_t block, size_t size, MenuFile& out)
{
    const int fonts = int16_t(u16(d, block + 0x12));
    const uint32_t fontList = u32(d, block + 0x48);
    for (int f = 0; f < fonts && fontList != 0; f++) {
        const size_t entry = block + fontList + 0x28 * size_t(f);
        const uint32_t letters = u32(d, entry);
        if (entry + 0x28 > block + size || letters == 0 || block + letters + 8 > block + size
            || std::memcmp(d.data() + block + letters, "SPC", 3) != 0)
            continue;
        MenuFont font;
        font.picture = textAt(d, entry + 4, 32);
        const size_t count = u16(d, block + letters + 4);
        const std::vector<int> codes = fontCodes(count);
        int left = 0;
        std::vector<int> row;  // the codes of the row so far
        int rowTop = 0x7FFF;
        for (size_t k = 0; k < count; k++) {
            const size_t e = block + letters + 8 + 8 * k;
            if (e + 8 > block + size)
                break;
            const int top = int16_t(u16(d, e)), right = int16_t(u16(d, e + 2));
            const int bottom = int16_t(u16(d, e + 4)), next = int16_t(u16(d, e + 6));
            if (codes[k] >= 0) {
                font.glyphs[codes[k]] = { left, top, right, bottom, 0 };
                row.push_back(codes[k]);
            }
            rowTop = std::min(rowTop, top);
            font.height = std::max(font.height, bottom - top);
            left = next;  // 0: the next one starts a row
            if (next == 0 || k + 1 == count) {
                for (int c : row)
                    font.glyphs[c].rowTop = rowTop;
                row.clear();
                rowTop = 0x7FFF;
            }
        }
        out.fonts.push_back(font);
    }
    // The texts (+0x50): 0, how many, where their list of places is
    const uint32_t texts = u32(d, block + 0x50);
    if (texts != 0 && texts + 12 <= size) {
        const uint32_t count = u32(d, block + texts + 4), list = u32(d, block + texts + 8);
        for (uint32_t k = 0; k < count && list + 4 * k + 4 <= size; k++) {
            const uint32_t at = u32(d, block + list + 4 * k);
            out.texts.push_back(at != 0 && at < size ? textAt(d, block + at, 256) : std::string());
        }
    }
}

} // namespace

bool loadMenuFile(const std::string& dataFolder, const std::string& relativePath, MenuFile& out)
{
    out = MenuFile();
    std::vector<uint8_t> d;
    if (!readDataFile((std::filesystem::path(dataFolder) / relativePath).string(), d) || d.size() < 12
        || std::memcmp(d.data(), "DRAM", 4) != 0)
        return false;
    const size_t size = u32(d, 4), pointers = u32(d, 8);
    if (12 + size > d.size())
        return false;
    readFontsAndTexts(d, 12, size, out);
    // The pictures: the DR2D chunk after the DRAM one (a G3DB may come between)
    const char tag[] = "DR2D";
    const auto found = std::search(d.begin() + std::min(d.size(), 12 + size + 4 * pointers), d.end(), tag, tag + 4);
    if (found != d.end() && !readPictures(d, size_t(found - d.begin()), out.pictures))
        return false;
    return true;
}

const MenuPicture* menuPicture(const MenuFile& menu, const std::string& name)
{
    for (const MenuPicture& p : menu.pictures)
        if (p.name == name)
            return &p;
    return nullptr;
}
