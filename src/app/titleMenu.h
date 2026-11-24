#pragma once

// The screens of menus: the title (New Game, Quit...), choosing Carnby or
// Aline, and the inventory during the game (our own: the game's is a 3D
// screen, menu\ingame2, not studied yet), drawn with the pictures and fonts
// of the game's own menu files (menu\menu2, menu\select2; core/menuFile). Our own layout and
// movement: the game moves its menus with pages of widgets in their DRAM, not
// studied yet. Its title has a model of seven rays of light (the G3DB of
// menu2) that it moves behind the logo; here they are drawn flat, swaying.

#include <cstdint>
#include <string>
#include <vector>

#include "menuFile.h"
#include "renderer.h"

enum class MenuScreen { Title, ChooseHero, Loading, Inventory, Playing };

// The entries of the title screen, in order
enum TitleEntry { kTitleNewGame, kTitleLoadGame, kTitleOptions, kTitleCredits, kTitleQuit, kTitleEntries };

struct TitleMenu {
    MenuFile title, heroes;
    MenuFile inGame;  // menu\ingame2: the font of the texts during the game (BMP_C7_ING)
    // The pictures used, side by side in one image for the renderer's layer
    std::vector<uint32_t> atlas;
    int atlasWidth = 0, atlasHeight = 0;
    struct Placed {
        std::string name;
        int x = 0, y = 0, width = 0, height = 0;  // in the image (what is used of the picture)
    };
    std::vector<Placed> placed;
    MenuScreen screen = MenuScreen::Title;
    int entry = kTitleNewGame;  // chosen on the title
    int hero = 0;               // chosen: 0 Carnby, 1 Aline
    bool playing = false;       // a game is going on (the title is its pause)
    bool canLoad = false;       // there is a saved game
    // The inventory: a line per item ("Small Steel Key", "Gas Cartridge  x3"),
    // the one chosen and the description of each
    std::vector<std::string> items;
    std::vector<std::string> descriptions;
    int item = 0;
    // The card of the hero chosen (FUN_004802c0, FUN_00480260): it slides in
    // from its side (x: where it is now, pixels of 640) and is typed a letter
    // at a time (letters typed so far; -1: start again)
    float cardX = 0;
    float cardLetters = -1;
};

// One step of the card of the hero chosen: it slides halfway to its place
// every frame of 30 a second, and types 40 letters a second. Returns how
// many letters (not spaces) it typed now: the game clicks for each (the
// sound 1 of select2's bank, intro\bip)
int stepHeroCard(TitleMenu& menu, float milliseconds);

// Reads the menu files and makes the image of their pictures. False if the
// files are missing (then there are no menus)
bool loadTitleMenu(const std::string& dataFolder, TitleMenu& out);

// What the layer shows for the screen of the menu (over a black screen),
// seconds after it was shown (it fades in and its light moves)
std::vector<OverlayRect> titleMenuRects(const TitleMenu& menu, float seconds);

// A text of the game during play (a message, or a question with its answers,
// the chosen one lit) in a box at the bottom of the screen, with the game's
// font of play (BMP_C7_ING of menu\ingame2; our own box: the game's,
// FUN_0043ce5f and FUN_0043d661, is not studied)
std::vector<OverlayRect> textBoxRects(const TitleMenu& menu, const std::string& text,
                                      const std::vector<std::string>& answers, int chosen);

// Whether a title entry can be chosen (the options are not done; loading
// needs a saved game)
bool titleEntryWorks(const TitleMenu& menu, int entry);

// What is under a point of the screen (pixels of 640 x 480): on the title,
// the entry (-1: none); choosing the hero, 0 Carnby or 1 Aline
int titleMenuItemAt(const TitleMenu& menu, float x, float y);
