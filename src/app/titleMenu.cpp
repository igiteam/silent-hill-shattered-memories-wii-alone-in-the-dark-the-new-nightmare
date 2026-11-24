#include "titleMenu.h"

#include <algorithm>
#include <cmath>

namespace {

// The pictures used and where they go in the image (2048 x 1024)
struct Wanted {
    bool heroes;  // of select2 (else menu2)
    const char* name;
    int x, y;
};
constexpr Wanted kWanted[] = {
    { false, "BMP_OLDBOOK", 0, 0 },      { true, "BMP_CARNBY", 512, 0 },     { true, "BMP_ALINE", 1024, 0 },
    { true, "BMP_CARNBY2", 1536, 0 },    { true, "BMP_ALINE2", 0, 512 },     { false, "LOGO_FRONT", 512, 512 },
    { false, "BMP_COURIER7", 512, 768 }, { false, "SPRITE_GLOW", 1024, 512 },
};
constexpr int kInGameFontX = 1024, kInGameFontY = 768;  // BMP_C7_ING of ingame2
constexpr int kAtlasWidth = 2048, kAtlasHeight = 1024;

// A ray of light of our own (the game's are a model, not drawn here): a soft
// cone, white fading away from its start, in the image at kRayX, kRayY
constexpr int kRayX = 1152, kRayY = 512, kRayLength = 256, kRayWidth = 64;
constexpr const char* kRay = "RAY";
// A white square, for plain rectangles drawn in turn with the pictures
constexpr int kWhiteX = 1408, kWhiteY = 512, kWhiteSize = 4;
constexpr const char* kWhite = "WHITE";

// Colours of the texts (the game's own grey 0xB4B4B4 for what is not chosen)
constexpr uint32_t kChosen = 0xFFFFFFFFu, kNotChosen = 0xFFB4B4B4u, kNotWorking = 0xFF505050u;

// The title's wheel (FUN_0047ae93): its entries one under the other from y
// 336, a line of the font each; the chosen one is at 336, with the one
// before above and the one after below, dimmer (as the game shows them)
constexpr float kChosenTop = 336;
constexpr uint32_t kNeighbour = 0xFF585858u, kNeighbourNotWorking = 0xFF2C2C2Cu;

const TitleMenu::Placed* placedPicture(const TitleMenu& menu, const std::string& name)
{
    for (const TitleMenu::Placed& p : menu.placed)
        if (p.name == name)
            return &p;
    return nullptr;
}

// A colour with its alpha made "amount" (0..1) of what it was
uint32_t faded(uint32_t colour, float amount)
{
    const float a = float(colour >> 24) * std::clamp(amount, 0.0f, 1.0f);
    return (colour & 0x00FFFFFFu) | uint32_t(a) << 24;
}

// A picture of the image drawn at x, y (640 x 480) with that size
void addPicture(std::vector<OverlayRect>& out, const TitleMenu& menu, const std::string& name, float x, float y,
                float width, float height, uint32_t colour = 0xFFFFFFFFu, float angle = 0)
{
    const TitleMenu::Placed* p = placedPicture(menu, name);
    if (!p)
        return;
    const float w = float(menu.atlasWidth), h = float(menu.atlasHeight);
    // Half a pixel in, so that the smooth reading does not take the neighbours
    out.push_back({ x, y, width, height, (float(p->x) + 0.5f) / w, (float(p->y) + 0.5f) / h,
                    (float(p->x + p->width) - 0.5f) / w, (float(p->y + p->height) - 0.5f) / h, colour, true, angle });
}

// How far the pen moves after a letter
float advance(const MenuFont& font, unsigned char c)
{
    const MenuGlyph& g = font.glyphs[c];
    // Each letter's box starts where the one before ended: its gap is in it
    return g.right > g.left ? float(g.right - g.left) : float(font.height) * 0.3f;
}

float textWidth(const MenuFont& font, const std::string& text)
{
    float width = 0;
    for (unsigned char c : text)
        width += advance(font, c);
    return width;
}

// A text in a font of the menu at its own size (or scaled), its top left
// at x, y
void addText(std::vector<OverlayRect>& out, const TitleMenu& menu, const MenuFont& font, const std::string& text,
             float x, float y, uint32_t colour, float scale = 1.0f)
{
    const TitleMenu::Placed* p = placedPicture(menu, font.picture);
    if (!p)
        return;
    const float w = float(menu.atlasWidth), h = float(menu.atlasHeight);
    for (unsigned char c : text) {
        const MenuGlyph& g = font.glyphs[c];
        if (g.right > g.left)
            out.push_back({ std::round(x), y + float(g.top - g.rowTop) * scale, float(g.right - g.left) * scale,
                            float(g.bottom - g.top) * scale,
                            float(p->x + g.left) / w, float(p->y + g.top) / h, float(p->x + g.right) / w,
                            float(p->y + g.bottom) / h, colour, true });
        x += advance(font, c) * scale;
    }
}

const MenuFont* fontNamed(const MenuFile& file, const std::string& picture)
{
    for (const MenuFont& f : file.fonts)
        if (f.picture == picture)
            return &f;
    return nullptr;
}

// A text of the menu file, or the English one if the file has no such text
std::string menuText(const MenuFile& file, const std::string& english)
{
    for (const std::string& t : file.texts)
        if (t == english)
            return t;
    return english;
}

// The card of a hero (0 Carnby, 1 Aline) in the texts of select2: one text
// starting with its name in capitals, its lines split by line breaks
std::vector<std::string> heroCard(const MenuFile& file, int hero)
{
    const std::string name = hero == 0 ? "EDWARD CARNBY" : "ALINE CEDRAC";
    std::vector<std::string> card;
    for (const std::string& t : file.texts) {
        if (t.compare(0, name.size(), name) != 0)
            continue;
        std::string line;
        for (char c : t + "\n") {
            if (c == '\n') {
                card.push_back(line);
                line.clear();
            } else
                line += c;
        }
        break;
    }
    if (card.empty())
        card.push_back(hero == 0 ? "Edward Carnby" : "Aline Cedrac");
    return card;
}

std::string entryName(const TitleMenu& menu, int entry)
{
    switch (entry) {
    case kTitleNewGame: return menuText(menu.title, menu.playing ? "Resume Game" : "New Game");
    case kTitleLoadGame: return menuText(menu.title, "Load Saved Game");
    case kTitleOptions: return menuText(menu.title, "Options");
    case kTitleCredits: return menuText(menu.title, "Credits");
    default: return menuText(menu.title, "Quit");
    }
}

// The entry of the wheel a row away from the chosen one (-1 above, 1 below)
int wheelEntry(const TitleMenu& menu, int row)
{
    return ((menu.entry + row) % kTitleEntries + kTitleEntries) % kTitleEntries;
}

// The ray of our own, drawn into the image
void makeRay(TitleMenu& out)
{
    for (int y = 0; y < kRayWidth; y++)
        for (int x = 0; x < kRayLength; x++) {
            const float along = float(x) / float(kRayLength - 1);                         // 0 at its start
            const float across = std::fabs(float(y) - (kRayWidth - 1) * 0.5f) / (kRayWidth * 0.5f);  // 0 in the middle
            const float wide = 0.15f + 0.85f * along;                                       // a cone
            const float side = std::clamp(1.0f - across / wide, 0.0f, 1.0f);
            const float alpha = side * side * std::pow(1.0f - along, 1.5f);
            out.atlas[size_t(kRayY + y) * kAtlasWidth + size_t(kRayX + x)] =
                uint32_t(alpha * 255.0f) << 24 | 0xFFE6D2u;  // a cold white (0xAABBGGRR)
        }
    out.placed.push_back({ kRay, kRayX, kRayY, kRayLength, kRayWidth });
    for (int y = 0; y < kWhiteSize; y++)
        for (int x = 0; x < kWhiteSize; x++)
            out.atlas[size_t(kWhiteY + y) * kAtlasWidth + size_t(kWhiteX + x)] = 0xFFFFFFFFu;
    out.placed.push_back({ kWhite, kWhiteX + 1, kWhiteY + 1, kWhiteSize - 2, kWhiteSize - 2 });
}

} // namespace

bool loadTitleMenu(const std::string& dataFolder, TitleMenu& out)
{
    out = TitleMenu();
    if (!loadMenuFile(dataFolder, "menu/menu2", out.title) || !loadMenuFile(dataFolder, "menu/select2", out.heroes))
        return false;
    out.atlasWidth = kAtlasWidth;
    out.atlasHeight = kAtlasHeight;
    out.atlas.assign(size_t(kAtlasWidth) * kAtlasHeight, 0);
    for (const Wanted& wanted : kWanted) {
        const MenuPicture* picture = menuPicture(wanted.heroes ? out.heroes : out.title, wanted.name);
        if (!picture)
            continue;
        // The logo and the glow are light on black: their black becomes see
        // through (alpha = the brightest of the colour), so that what is
        // behind (the rays) shows
        const bool light = picture->name == "LOGO_FRONT" || picture->name == "SPRITE_GLOW";
        for (int y = 0; y < picture->height && wanted.y + y < kAtlasHeight; y++)
            for (int x = 0; x < picture->width && wanted.x + x < kAtlasWidth; x++) {
                uint32_t c = picture->rgba[size_t(y) * picture->width + size_t(x)];
                if (light && (c >> 24) != 0) {
                    const uint32_t r = c & 0xFF, g = c >> 8 & 0xFF, b = c >> 16 & 0xFF;
                    const uint32_t a = std::max({ r, g, b });
                    c = a == 0 ? 0 : a << 24 | (b * 255 / a) << 16 | (g * 255 / a) << 8 | (r * 255 / a);
                }
                out.atlas[size_t(wanted.y + y) * kAtlasWidth + size_t(wanted.x + x)] = c;
            }
        out.placed.push_back({ picture->name, wanted.x, wanted.y, picture->usedWidth, picture->usedHeight });
    }
    makeRay(out);
    if (loadMenuFile(dataFolder, "menu/ingame2", out.inGame))
        if (const MenuPicture* font = menuPicture(out.inGame, "BMP_C7_ING")) {
            for (int y = 0; y < font->height && kInGameFontY + y < kAtlasHeight; y++)
                for (int x = 0; x < font->width && kInGameFontX + x < kAtlasWidth; x++)
                    out.atlas[size_t(kInGameFontY + y) * kAtlasWidth + size_t(kInGameFontX + x)] =
                        font->rgba[size_t(y) * font->width + size_t(x)];
            out.placed.push_back({ font->name, kInGameFontX, kInGameFontY, font->usedWidth, font->usedHeight });
        }
    return true;
}

bool titleEntryWorks(const TitleMenu& menu, int entry)
{
    return entry == kTitleNewGame || entry == kTitleQuit || (entry == kTitleLoadGame && menu.canLoad);
}

int titleMenuItemAt(const TitleMenu& menu, float x, float y)
{
    if (menu.screen == MenuScreen::ChooseHero)
        return x < 320 ? 0 : 1;
    const MenuFont* big = fontNamed(menu.title, "BMP_OLDBOOK");
    if (menu.screen != MenuScreen::Title || !big)
        return -1;
    for (int row = -1; row <= 1; row++) {
        const int k = wheelEntry(menu, row);
        const float width = textWidth(*big, entryName(menu, k)), top = kChosenTop + float(row * big->height);
        if (x >= 320 - width / 2 - 10 && x <= 320 + width / 2 + 10 && y >= top && y < top + float(big->height))
            return k;
    }
    return -1;
}

namespace {

// A text cut into lines no wider than "width" in that font (at its spaces)
std::vector<std::string> wrapLines(const MenuFont& font, const std::string& text, float width)
{
    std::vector<std::string> lines;
    std::string line, word;
    auto flush = [&]() {
        if (word.empty())
            return;
        const std::string longer = line.empty() ? word : line + " " + word;
        if (!line.empty() && textWidth(font, longer) > width) {
            lines.push_back(line);
            line = word;
        } else
            line = longer;
        word.clear();
    };
    for (char c : text) {
        if (c == ' ')
            flush();
        else
            word += c;
    }
    flush();
    if (!line.empty())
        lines.push_back(line);
    return lines;
}

// The inventory over the game (dimmed): its items on the left, the chosen
// one's description below
void addInventory(std::vector<OverlayRect>& rects, const TitleMenu& menu, float fade)
{
    const MenuFont* big = fontNamed(menu.title, "BMP_OLDBOOK");
    const MenuFont* small = fontNamed(menu.title, "BMP_COURIER7");
    if (!big || !small)
        return;
    const std::string title = "Inventory";
    addText(rects, menu, *big, title, 320 - textWidth(*big, title) / 2, 24, faded(kChosen, fade));
    if (menu.items.empty()) {
        const std::string nothing = "You carry nothing.";
        addText(rects, menu, *small, nothing, 320 - textWidth(*small, nothing) / 2, 120, faded(kNotChosen, fade));
        return;
    }
    // At most 12 lines, around the chosen one
    constexpr int kLines = 12;
    const int count = int(menu.items.size());
    const int first = std::clamp(menu.item - kLines / 2, 0, std::max(count - kLines, 0));
    float y = 80;
    for (int k = first; k < count && k < first + kLines; k++) {
        if (k == menu.item)
            addPicture(rects, menu, "SPRITE_GLOW", 50, y - 4, 300, float(small->height) + 8, faded(0x80FFFFFFu, fade));
        addText(rects, menu, *small, menu.items[size_t(k)], 70, y, faded(k == menu.item ? kChosen : kNotChosen, fade));
        y += float(small->height) + 4;
    }
    if (size_t(menu.item) < menu.descriptions.size()) {
        rects.push_back({ 40, 360, 560, 104, 0, 0, 0, 0, faded(0xA0000000u, fade), false });
        float line = 370;
        for (const std::string& text : wrapLines(*small, menu.descriptions[size_t(menu.item)], 540)) {
            addText(rects, menu, *small, text, 50, line, faded(kNotChosen, fade));
            line += float(small->height) + 2;
            if (line > 450)
                break;
        }
    }
}

} // namespace

std::vector<OverlayRect> textBoxRects(const TitleMenu& menu, const std::string& text,
                                      const std::vector<std::string>& answers, int chosen)
{
    std::vector<OverlayRect> rects;
    const MenuFont* font = fontNamed(menu.inGame, "BMP_C7_ING");
    if (!font)
        font = fontNamed(menu.title, "BMP_COURIER7");
    if (!font)
        return rects;
    constexpr float kLeft = 40, kWidth = 560, kMargin = 14;
    const float line = float(font->height) + 4;
    const std::vector<std::string> lines = wrapLines(*font, text, kWidth - 2 * kMargin);
    const float height = line * float(lines.size() + (answers.empty() ? 0 : 1)) + 2 * kMargin;
    const float top = 466 - height;
    rects.push_back({ kLeft, top, kWidth, height, 0, 0, 0, 0, 0xB0000000u, false });
    // A thin frame
    for (const OverlayRect& edge : { OverlayRect{ kLeft, top, kWidth, 1, 0, 0, 0, 0, 0x80B4B4B4u, false },
                                     OverlayRect{ kLeft, top + height - 1, kWidth, 1, 0, 0, 0, 0, 0x80B4B4B4u, false },
                                     OverlayRect{ kLeft, top, 1, height, 0, 0, 0, 0, 0x80B4B4B4u, false },
                                     OverlayRect{ kLeft + kWidth - 1, top, 1, height, 0, 0, 0, 0, 0x80B4B4B4u, false } })
        rects.push_back(edge);
    float y = top + kMargin;
    for (const std::string& l : lines) {
        addText(rects, menu, *font, l, kLeft + kMargin, y, kChosen);
        y += line;
    }
    // The answers side by side, centred
    float total = 0;
    for (const std::string& a : answers)
        total += textWidth(*font, a) + 40;
    float x = 320 - (total - 40) / 2;
    for (size_t k = 0; k < answers.size(); k++) {
        if (int(k) == chosen)
            addPicture(rects, menu, "SPRITE_GLOW", x - 16, y - 4, textWidth(*font, answers[k]) + 32,
                       float(font->height) + 8, 0x90FFFFFFu);
        addText(rects, menu, *font, answers[k], x, y, int(k) == chosen ? kChosen : kNotWorking);
        x += textWidth(*font, answers[k]) + 40;
    }
    return rects;
}

int stepHeroCard(TitleMenu& menu, float milliseconds)
{
    const float target = menu.hero == 0 ? 0.0f : 320.0f;
    if (menu.cardLetters < 0) {
        // It comes from off the screen on its side (FUN_004803e0)
        menu.cardX = menu.hero == 0 ? -320.0f : 960.0f;  // half a screen off it (FUN_0047f633)
        menu.cardLetters = 0;
    }
    menu.cardX = target + (menu.cardX - target) * std::pow(0.5f, milliseconds * 0.03f);
    std::string all;
    for (const std::string& line : heroCard(menu.heroes, menu.hero))
        all += line + "\n";
    const int before = int(menu.cardLetters);
    menu.cardLetters = std::min(menu.cardLetters + milliseconds * 0.04f, float(all.size()));
    int typed = 0;
    for (int k = before; k < int(menu.cardLetters); k++)
        if (all[size_t(k)] != ' ' && all[size_t(k)] != '\n')
            typed++;
    return typed;
}

std::vector<OverlayRect> titleMenuRects(const TitleMenu& menu, float seconds)
{
    std::vector<OverlayRect> rects;
    const float fade = std::clamp(seconds / 0.8f, 0.0f, 1.0f);  // it appears in 0.8 s
    if (menu.screen == MenuScreen::Inventory) {
        rects.push_back({ 0, 0, 640, 480, 0, 0, 0, 0, faded(0xD0000000u, std::min(seconds / 0.25f, 1.0f)), false });
        addInventory(rects, menu, std::min(seconds / 0.25f, 1.0f));
        return rects;
    }
    rects.push_back({ 0, 0, 640, 480, 0, 0, 0, 0, 0xFF000000u, false });  // black under everything
    const MenuFont* big = fontNamed(menu.title, "BMP_OLDBOOK");
    const MenuFont* small = fontNamed(menu.title, "BMP_COURIER7");
    if (menu.screen == MenuScreen::Title) {
        // Seven rays of light from behind the logo, turning slowly round it
        // and swaying, and a glow that beats
        constexpr float kPi = 3.14159265f;
        for (int k = 0; k < 7; k++) {
            const float base = float(k) * 2 * kPi / 7 + seconds * 0.12f;
            const float sway = 0.25f * std::sin(seconds * 0.45f + float(k) * 1.7f);
            const float strength = 0.30f + 0.18f * std::sin(seconds * 0.8f + float(k) * 2.3f);
            addPicture(rects, menu, kRay, 320, 140 - 40, 330, 80, faded(0xFFFFFFFFu, strength * fade), base + sway);
        }
        const float beat = 0.30f + 0.12f * std::sin(seconds * 2.0f);
        addPicture(rects, menu, "SPRITE_GLOW", 130, 10, 380, 250, faded(0xFFFFFFFFu, beat * fade));
        addPicture(rects, menu, "LOGO_FRONT", 141, 30, 358, 226, faded(0xFFFFFFFFu, fade));
        if (!big)
            return rects;
        for (int row = -1; row <= 1; row++) {
            const int k = wheelEntry(menu, row);
            const std::string name = entryName(menu, k);
            const float width = textWidth(*big, name), top = kChosenTop + float(row * big->height);
            const bool works = titleEntryWorks(menu, k);
            if (row == 0 && works) {
                // The chosen one, with a soft glow behind
                const float glow = 0.25f + 0.10f * std::sin(seconds * 4.0f);
                addPicture(rects, menu, "SPRITE_GLOW", 320 - width / 2 - 30, top - 8, width + 60,
                           float(big->height) + 16, faded(0xFFFFFFFFu, glow * fade));
            }
            const uint32_t colour = row == 0 ? (works ? kChosen : kNotWorking) : (works ? kNeighbour : kNeighbourNotWorking);
            addText(rects, menu, *big, name, 320 - width / 2, top, faded(colour, fade));
        }
    } else if (menu.screen == MenuScreen::ChooseHero) {
        // Aline on the left, Carnby on the right (the pictures of select2's
        // page). The chosen one's card (FUN_0047f633: Carnby's on the left,
        // Aline's on the right, from y 100) slides over the other's half from
        // off the screen, a panel that darkens what is under it by 200 - 32 x
        // its distance (FUN_004802c0: black while it comes, then 200 of
        // 255); its text is typed from 10 pixels in, in the game's grey
        addPicture(rects, menu, "BMP_ALINE", 0, 0, 320, 480, faded(0xFFFFFFFFu, fade));
        addPicture(rects, menu, "BMP_CARNBY", 320, 0, 320, 480, faded(0xFFFFFFFFu, fade));
        if (small) {
            const float target = menu.hero == 0 ? 0.0f : 320.0f;
            const float grey = std::clamp(200.0f - 32.0f * std::fabs(menu.cardX - target), 0.0f, 200.0f) / 255.0f;
            addPicture(rects, menu, kWhite, menu.cardX, 0, 321, 480, faded(0xFF000000u, (1.0f - grey) * fade));
            int left = int(std::max(menu.cardLetters, 0.0f));
            float y = 100;
            for (const std::string& line : heroCard(menu.heroes, menu.hero)) {
                const std::string shown = line.substr(0, size_t(std::clamp(left, 0, int(line.size()))));
                left -= int(line.size()) + 1;
                addText(rects, menu, *small, shown, menu.cardX + 10, y, faded(kNotChosen, fade));
                y += float(small->height);
                if (left <= 0)
                    break;
            }
            // Under the pictures, in the middle (select2's page: y 400)
            const std::string choose = menuText(menu.heroes, "Choose a character");
            addText(rects, menu, *small, choose, 320 - textWidth(*small, choose) / 2, 400, faded(kNotChosen, fade));
        }
    } else if (menu.screen == MenuScreen::Loading && small) {
        const std::string loading = menuText(menu.heroes, "LOADING...");
        addText(rects, menu, *small, loading, 320 - textWidth(*small, loading) / 2, 230, faded(kChosen, fade));
    }
    return rects;
}
