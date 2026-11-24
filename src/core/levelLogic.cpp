#include "levelLogic.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

#include "dataFile.h"

namespace {

constexpr uint32_t kMagic = 0xDEADBEEF, kVersion = 0x0131554C;

// Places in the header of the DRAM block (FUN_0044a39d)
constexpr size_t kLevelScript = 0x14;
constexpr size_t kNoActionScript = 0x1C;
constexpr size_t kTexts = 0x374;             // points to: u32 0, number of texts, list of places
constexpr size_t kPassTriggerCount = 0x390;  // i16
constexpr size_t kZoneTriggerCount = 0x392;  // i16
constexpr size_t kEntryCount = 0x394;        // i16
constexpr size_t kMarkerCount = 0x3A6;       // i16
constexpr size_t kEventCount = 0x3AA;        // i16
constexpr size_t kMarkers = 0x3D8;           // 0x1C bytes each
constexpr size_t kObjects = 0x3BC;           // 0xB4 bytes each
constexpr size_t kObjectSize = 0xB4;
constexpr size_t kActors = 0x3B8;            // 0xD0 bytes each
constexpr size_t kActorSize = 0xD0;
constexpr size_t kEvents = 0x3E0;            // 8 bytes each
// Numbers of the lists of things, in the order of LevelList
constexpr size_t kListCounts[kLevelLists] = { 0x396, 0x398, 0x39A, 0x39C, 0x39E };
constexpr size_t kPassTriggers = 0x3AC;      // 0x10 bytes each
constexpr size_t kZoneTriggers = 0x3B0;      // 0x20 bytes each
constexpr size_t kEntries = 0x3B4;           // 0x24 bytes each
constexpr size_t kCollisionMap = 0x3E4;
constexpr size_t kTriggerMap = 0x3E8;
constexpr size_t kHeaderSize = 0x430;
constexpr size_t kPassTriggerSize = 0x10, kZoneTriggerSize = 0x20, kEntrySize = 0x24;
constexpr int kForcedEntry = 100;
constexpr uint32_t kMaxTexts = 4096;

uint32_t readU32(const uint8_t* p)
{
    uint32_t value;
    std::memcpy(&value, p, 4);
    return value;
}

int16_t readI16(const uint8_t* p)
{
    int16_t value;
    std::memcpy(&value, p, 2);
    return value;
}

float readFloat(const uint8_t* p)
{
    float value;
    std::memcpy(&value, p, 4);
    return value;
}

// A count of the header (i16), as a size, at most the number of things read
size_t headerCount(const uint8_t* block, size_t at, size_t most)
{
    return std::min(size_t(std::max<int16_t>(readI16(block + at), 0)), most);
}

} // namespace

bool loadLevelLogic(const std::string& dataFolder, int level, const std::string& fileName, LevelLogic& out)
{
    out = LevelLogic{};
    std::vector<uint8_t> data;
    const std::filesystem::path path = std::filesystem::path(dataFolder) / "levels" / std::to_string(level) / fileName;
    if (!readDataFile(path.string(), data) || data.size() < 12 || std::memcmp(data.data(), "DRAM", 4) != 0)
        return false;
    const size_t size = readU32(&data[4]);
    if (size < kHeaderSize || 12 + size > data.size())
        return false;
    out.block.assign(data.begin() + 12, data.begin() + 12 + std::ptrdiff_t(size));
    const uint8_t* block = out.block.data();
    if (readU32(block) != kMagic || readU32(block + 4) != kVersion) {
        out.block.clear();
        return false;
    }

    out.levelScript = readU32(block + kLevelScript);
    out.noActionScript = readU32(block + kNoActionScript);
    const uint32_t map = readU32(block + kCollisionMap);
    out.hasCollision = map != 0 && readCollisionMap(block, size, map, out.collision);

    // The trigger walls and zones, and what each does
    if (readTriggerMap(block, size, readU32(block + kTriggerMap), out.triggers)) {
        const uint32_t passes = readU32(block + kPassTriggers);
        const size_t passCount = headerCount(block, kPassTriggerCount, out.triggers.walls.size());
        if (passes != 0 && passes + kPassTriggerSize * passCount <= size)
            for (size_t k = 0; k < passCount; k++) {
                const uint8_t* p = block + passes + kPassTriggerSize * k;
                out.passTriggers.push_back({ readI16(p + 4), readU32(p + 8), readU32(p + 12) });
            }
        const uint32_t zones = readU32(block + kZoneTriggers);
        const size_t zoneCount = headerCount(block, kZoneTriggerCount, out.triggers.zones.size());
        if (zones != 0 && zones + kZoneTriggerSize * zoneCount <= size)
            for (size_t k = 0; k < zoneCount; k++) {
                const uint8_t* p = block + zones + kZoneTriggerSize * k;
                out.zoneTriggers.push_back(
                    { readU32(p), readU32(p + 4), readU32(p + 8), readU32(p + 12), readI16(p + 0x14), readI16(p + 0x16) });
            }
    }

    // The ways in: position, angles (the one about z is the heading), level
    // one comes from, camera, forced, script
    const uint32_t entries = readU32(block + kEntries);
    const size_t entryCount = std::max<int16_t>(readI16(block + kEntryCount), 0);
    if (entries != 0 && entries + kEntrySize * entryCount <= size)
        for (size_t k = 0; k < entryCount; k++) {
            const uint8_t* p = block + entries + kEntrySize * k;
            LevelEntry entry;
            for (int i = 0; i < 3; i++)
                entry.place[i] = readFloat(p + 4 * size_t(i));
            entry.heading = readFloat(p + 0x14);
            entry.fromLevel = readI16(p + 0x18);
            entry.camera = readI16(p + 0x1A);
            entry.forced = readI16(p + 0x1C) == kForcedEntry;
            entry.script = readU32(p + 0x20);
            out.entries.push_back(entry);
        }

    // The marked places: position, then angles
    const uint32_t markers = readU32(block + kMarkers);
    const size_t markerCount = std::max<int16_t>(readI16(block + kMarkerCount), 0);
    if (markers != 0 && markers + 0x1C * markerCount <= size)
        for (size_t k = 0; k < markerCount; k++) {
            LevelMarker marker;
            for (int i = 0; i < 3; i++) {
                marker.place[i] = readFloat(block + markers + 0x1C * k + 4 * size_t(i));
                marker.angles[i] = readFloat(block + markers + 0x1C * k + 12 + 4 * size_t(i));
            }
            out.markers.push_back(marker);
        }

    // The scripts by number
    const uint32_t events = readU32(block + kEvents);
    const size_t eventCount = std::max<int16_t>(readI16(block + kEventCount), 0);
    if (events != 0 && events + 8 * eventCount <= size)
        for (size_t k = 0; k < eventCount; k++)
            out.events.push_back({ readI16(block + events + 8 * k), readU32(block + events + 8 * k + 4) });

    for (int k = 0; k < kLevelLists; k++)
        out.listSizes[k] = std::max<int16_t>(readI16(block + kListCounts[k]), 0);

    // The things with a model (FUN_0046850c): place, angles, model, animation,
    // flags
    const uint32_t objects = readU32(block + kObjects);
    const size_t objectCount = size_t(out.listSizes[kListObjects]);
    if (objects != 0 && objects + kObjectSize * objectCount <= size)
        for (size_t k = 0; k < objectCount; k++) {
            const uint8_t* p = block + objects + kObjectSize * k;
            LevelObject object;
            for (int i = 0; i < 3; i++) {
                object.place[i] = readFloat(p + 4 * size_t(i));
                object.angles[i] = readFloat(p + 12 + 4 * size_t(i));
            }
            object.model = readI16(p + 0x18);
            object.animation = readI16(p + 0x1C);
            std::memcpy(&object.flags, p + 0x20, 2);
            object.visible = (object.flags & kObjectSeen) != 0;
            object.shadowVisible = (object.flags & kObjectShadowSeen) != 0;
            object.animating = (object.flags & kObjectAnimated) != 0;
            out.objects.push_back(object);
        }

    // The particle systems put somewhere (FUN_0044f5b5)
    const uint32_t things = readU32(block + 0x3C4);
    const size_t thingCount = size_t(out.listSizes[kListThings]);
    if (things != 0 && things + 0x1C * thingCount <= size)
        for (size_t k = 0; k < thingCount; k++) {
            const uint8_t* p = block + things + 0x1C * k;
            LevelThing thing;
            thing.system = int8_t(p[1]);
            std::memcpy(&thing.flags, p + 2, 2);
            for (int i = 0; i < 3; i++) {
                thing.place[i] = readFloat(p + 4 + 4 * size_t(i));
                thing.angles[i] = readFloat(p + 0x10 + 4 * size_t(i));
            }
            out.things.push_back(thing);
        }
    out.flags = readU32(block + 0x10);
    for (int i = 0; i < 3; i++) {
        out.ambientLit[i] = block[0x24 + i];
        out.ambientDark[i] = block[0x28 + i];
    }
    out.rainSystem = int8_t(block[0x3F1]);
    out.splashSystem = int8_t(block[0x3F3]);

    // The slots of the sound bank for the general sounds and the actors'
    for (size_t k = 0; k < out.generalSounds.size(); k++)
        out.generalSounds[k] = readI16(block + 0x34 + 2 * k);
    for (size_t kind = 0; kind < out.actorSounds.size(); kind++)
        for (size_t k = 0; k < out.actorSounds[kind].size(); k++)
            out.actorSounds[kind][k] = readI16(block + 0x74 + 0x30 * kind + 2 * k);

    const int floorKinds = readI16(block + 0x3A8);
    const uint32_t floors = readU32(block + 0x3DC);
    if (floorKinds > 0 && floors != 0 && floors + 0x90 * size_t(floorKinds) <= size)
        for (int k = 0; k < floorKinds; k++) {
            std::array<int16_t, 72> slots;
            for (size_t i = 0; i < slots.size(); i++)
                slots[i] = readI16(block + floors + 0x90 * size_t(k) + 2 * i);
            out.floorSounds.push_back(slots);
        }

    // The actors (FUN_00450c42): model, start state ("npc start state": 0
    // away, 1 here, 2 here with behaviour 2), kind, place, angles
    const uint32_t actors = readU32(block + kActors);
    const size_t actorCount = size_t(out.listSizes[kListActors]);
    if (actors != 0 && actors + kActorSize * actorCount <= size)
        for (size_t k = 0; k < actorCount; k++) {
            const uint8_t* p = block + actors + kActorSize * k;
            LevelActor actor;
            actor.model = readI16(p);
            actor.kind = readI16(p + 8);
            for (int i = 0; i < 3; i++) {
                actor.place[i] = readFloat(p + 0x18 + 4 * size_t(i));
                actor.angles[i] = readFloat(p + 0x24 + 4 * size_t(i));
            }
            const int start = readI16(p + 4);
            actor.presence = start == 1 || start == 2 ? ActorPresence::Here : ActorPresence::Away;
            actor.shown = actor.presence == ActorPresence::Here;
            actor.behaviour = start == 2 ? 2 : 1;
            actor.body.x = actor.place[0];
            actor.body.y = actor.place[1];
            actor.body.z = actor.place[2];
            actor.body.heading = actor.angles[2];
            out.actors.push_back(actor);
        }

    // The texts: +0x374 points to u32 0, their number and the list of their
    // places, each text ending with a zero
    const uint32_t textList = readU32(block + kTexts);
    const uint32_t textCount = textList != 0 && textList + 12 <= size ? readU32(block + textList + 4) : 0;
    const uint32_t texts = textCount != 0 ? readU32(block + textList + 8) : 0;
    if (textCount <= kMaxTexts && texts != 0 && texts + 4 * size_t(textCount) <= size)
        for (uint32_t k = 0; k < textCount; k++) {
            const uint32_t at = readU32(block + texts + 4 * size_t(k));
            const uint8_t* end = at < size ? static_cast<const uint8_t*>(std::memchr(block + at, 0, size - at)) : nullptr;
            out.texts.push_back(end ? std::string(reinterpret_cast<const char*>(block + at), size_t(end - (block + at)))
                                    : std::string());
        }
    return true;
}

const LevelEntry* chooseEntry(const LevelLogic& logic, int fromLevel)
{
    for (const LevelEntry& entry : logic.entries)
        if (entry.forced)
            return &entry;
    for (const LevelEntry& entry : logic.entries)
        if (fromLevel < 0 ? entry.fromLevel < 0 : entry.fromLevel == fromLevel)
            return &entry;
    return logic.entries.empty() ? nullptr : &logic.entries[0];
}
