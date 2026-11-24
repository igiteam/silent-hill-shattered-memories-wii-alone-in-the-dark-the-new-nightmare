#include "itemFile.h"

#include <string>

#include "menuFile.h"

namespace {

bool startsWith(const std::string& s, const char* start)
{
    return s.compare(0, std::char_traits<char>::length(start), start) == 0;
}

// The name in "You take a Small Steel Key.": what follows "You take" and
// "a", "an" or "the", without the final dot
std::string nameFromTaken(std::string s)
{
    for (const char* start : { "You take ", "a ", "an ", "the ", "A ", "An ", "The " })
        if (startsWith(s, start))
            s.erase(0, std::char_traits<char>::length(start));
    while (!s.empty() && (s.back() == '.' || s.back() == ' '))
        s.pop_back();
    return s;
}

} // namespace

bool loadItemInfo(const std::string& dataFolder, int item, ItemInfo& out)
{
    out = ItemInfo();
    MenuFile file;
    if (!loadMenuFile(dataFolder, "item/IT3D_" + std::to_string(item), file))
        return false;
    if (!file.texts.empty())
        out.description = file.texts[0];
    if (file.texts.size() > 1) {
        const std::string& taken = file.texts[1];
        const size_t bar = taken.find('|');
        out.taken = taken.substr(0, bar);
        if (bar != std::string::npos)
            out.takenSeveral = taken.substr(bar + 1);
    }
    out.name = nameFromTaken(out.taken);
    return true;
}

std::string takenText(const ItemInfo& info, int count)
{
    if (count <= 1 || info.takenSeveral.empty())
        return info.taken;
    // Its %d is the count (put by hand: the text comes from the data)
    std::string text = info.takenSeveral;
    const size_t at = text.find("%d");
    if (at != std::string::npos)
        text.replace(at, 2, std::to_string(count));
    return text;
}
