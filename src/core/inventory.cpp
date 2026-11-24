#include "inventory.h"

#include <algorithm>

namespace {

constexpr int kRationsItem = 0x7E;   // counts five each
constexpr int kRefillItem = 0x7C;    // fills item 0x5A
constexpr int kGaugeItem = 0x5A;     // a gauge up to 100
constexpr int kAddedItem = 0x77;     // adds what it is given
constexpr size_t kMostItems = 0x53;  // room of the game's list

int findItem(const Inventory& inventory, int item)
{
    for (size_t k = 0; k < inventory.items.size(); k++)
        if (inventory.items[k].item == item)
            return int(k);
    return -1;
}

} // namespace

void takeItem(Inventory& inventory, int item, int count, const std::vector<WeaponCharacteristics>& weapons)
{
    if (item == kRationsItem)
        count *= 5;
    if (item == kRefillItem) {
        const int gauge = findItem(inventory, kGaugeItem);
        if (gauge >= 0 && inventory.items[size_t(gauge)].count != 100)
            inventory.items[size_t(gauge)].count = std::min(inventory.items[size_t(gauge)].count + 0x21, 100);
        return;
    }
    int at = findItem(inventory, item);
    if (at < 0) {
        if (inventory.items.size() >= kMostItems)
            return;
        inventory.items.push_back({ item, 0 });
        at = int(inventory.items.size()) - 1;
    }
    InventoryItem& slot = inventory.items[size_t(at)];
    if ((item == 0x1A || item == 0x97) && slot.count == 0)
        count = 4;
    inventory.chosen = item;
    if (item == kAddedItem) {
        slot.count += count;
        return;
    }
    if (item == kGaugeItem) {
        slot.count = 100;
        return;
    }
    for (const WeaponCharacteristics& w : weapons)
        if (w.ammunitionItem == item) {
            slot.count += count * w.shotsPerClip * w.shotsPerBox;
            return;
        }
    slot.count += count;
}

void dropItem(Inventory& inventory, int item)
{
    const int at = findItem(inventory, item);
    if (at >= 0)
        inventory.items.erase(inventory.items.begin() + at);
}

bool hasItem(const Inventory& inventory, int item)
{
    return findItem(inventory, item) >= 0;
}

int itemCount(const Inventory& inventory, int item)
{
    const int at = findItem(inventory, item);
    return at >= 0 ? inventory.items[size_t(at)].count : 0;
}

void setItemCount(Inventory& inventory, int item, int count)
{
    const int at = findItem(inventory, item);
    if (at >= 0)
        inventory.items[size_t(at)].count = count;
}

void noteNumber(Inventory& inventory, int number)
{
    if (std::find(inventory.noted.begin(), inventory.noted.end(), number) == inventory.noted.end())
        inventory.noted.push_back(number);
}
