#include "radioScene.h"

#include <algorithm>

#include "dataFile.h"

namespace {

uint16_t read16(const std::vector<uint8_t>& d, size_t at)
{
    return at + 2 <= d.size() ? uint16_t(d[at] | d[at + 1] << 8) : 0xFFFF;
}

uint32_t read32(const std::vector<uint8_t>& d, size_t at)
{
    return at + 4 <= d.size() ? uint32_t(d[at] | d[at + 1] << 8 | d[at + 2] << 16 | uint32_t(d[at + 3]) << 24) : 0;
}

} // namespace

bool RadioScene::start(const std::string& path, int scriptAfter, RadioActions& actions)
{
    stop();
    if (!readDataFile(path, data_) || data_.size() < 0x10)
        return false;
    const uint32_t table = read32(data_, 4), count = read32(data_, 8);
    for (uint32_t k = 0; k < count; k++)
        blocks_.push_back(read32(data_, 0x10 + size_t(table) + 4 * size_t(k)));
    if (blocks_.empty())
        return false;
    running_ = true;
    scriptAfter_ = scriptAfter;
    startBlock(0, actions);
    return true;
}

void RadioScene::stop()
{
    for (Watcher& w : watchers_)
        w = Watcher();
    blocks_.clear();
    running_ = false;
    scriptAfter_ = -1;
}

int32_t RadioScene::argument(size_t at, int index) const
{
    return int32_t(read32(data_, at + 4 * size_t(index)));
}

void RadioScene::noteMissing(int order)
{
    if (std::find(missing_.begin(), missing_.end(), order) == missing_.end())
        missing_.push_back(order);
}

void RadioScene::startBlock(int block, RadioActions& actions)
{
    if (block < 0 || size_t(block) >= blocks_.size())
        return;
    size_t at = 0x10 + blocks_[size_t(block)];
    for (;;) {
        uint16_t order = read16(data_, at);
        at += 2;
        if (order == 0xFFFF)
            return;
        Watcher* watcher = nullptr;
        if (order & 0x8000) {
            order &= 0x7FFF;
            const int then = read16(data_, at);
            at += 2;
            for (Watcher& w : watchers_)
                if (w.order < 0) {
                    watcher = &w;
                    break;
                }
            if (watcher) {
                *watcher = Watcher();
                watcher->order = order;
                watcher->block = then;
            }
        }
        const uint16_t size = read16(data_, at);
        at += 2;
        if (watcher)
            watcher->arguments = at;
        startOrder(order, at, actions);
        at += size;
    }
}

// The start of each order (the first function of each pair of 0x4C5B68)
void RadioScene::startOrder(int order, size_t at, RadioActions& actions)
{
    switch (order) {
    case 3:  // wait (only its "still going?")
        break;
    case 4:
        actions.requestCamera(argument(at, 1));
        break;
    case 5:
        actions.leadHero(true);
        break;
    case 6:
        actions.leadHero(false);
        break;
    case 7:
        actions.holdHeroButtons((argument(at, 1) ? 1u : 0u) | (argument(at, 2) ? 2u : 0u) | (argument(at, 3) ? 4u : 0u));
        break;
    case 8:
        actions.walkHeroTo(argument(at, 1));
        break;
    case 9:
        actions.turnHeroTo(argument(at, 1), argument(at, 2));
        break;
    case 10:
        actions.playHeroAnimation(argument(at, 1), argument(at, 2) != 0, argument(at, 3) != 0);
        break;
    case 11:  // a mark of the game's text box (+0xA06, FUN_0043c89f): not known
        break;
    case 12:
        actions.setCinemaBands(argument(at, 1) != 0);
        break;
    case 13:
        actions.playVoice(argument(at, 1));
        break;
    case 16:
        actions.giveItem(argument(at, 1));
        break;
    case 20:
        actions.showThing(argument(at, 0), argument(at, 1));
        break;
    case 28:
        actions.setFlashlight(argument(at, 1) != 0);
        break;
    case 37:
        actions.putHeroAt(argument(at, 1));
        break;
    default:
        noteMissing(order);
        break;
    }
}

// The second function of each pair: false once it is over (none: over at once)
bool RadioScene::stillGoing(Watcher& w, RadioActions& actions)
{
    switch (w.order) {
    case 3:
        return w.time < float(argument(w.arguments, 1));
    case 4:
        return actions.shownCamera() != argument(w.arguments, 1);
    case 8:
        return actions.heroWalking();
    case 9:
        return actions.heroTurning();
    case 10:
        return actions.heroAnimationPlaying();
    case 13:
        return actions.voicePlaying();
    default:
        return false;
    }
}

void RadioScene::step(float milliseconds, RadioActions& actions, int& scriptAfter)
{
    scriptAfter = -1;
    if (!running_)
        return;
    std::vector<int> over;
    for (Watcher& w : watchers_) {
        if (w.order < 0)
            continue;
        w.time += milliseconds;
        if (!stillGoing(w, actions)) {
            over.push_back(w.block);
            w = Watcher();
        }
    }
    for (int block : over)
        startBlock(block, actions);
    // No watcher left: the scene is over
    if (std::none_of(std::begin(watchers_), std::end(watchers_), [](const Watcher& w) { return w.order >= 0; })) {
        scriptAfter = scriptAfter_;
        stop();
    }
}
