#include "particles.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include "dataFile.h"

namespace {

constexpr float kPi = 3.14159265f;

uint32_t read32(const std::vector<uint8_t>& d, size_t at)
{
    return at + 4 <= d.size() ? uint32_t(d[at] | d[at + 1] << 8 | d[at + 2] << 16 | uint32_t(d[at + 3]) << 24) : 0;
}

int16_t read16(const std::vector<uint8_t>& d, size_t at)
{
    return at + 2 <= d.size() ? int16_t(d[at] | d[at + 1] << 8) : 0;
}

float readFloat(const std::vector<uint8_t>& d, size_t at)
{
    const uint32_t bits = read32(d, at);
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

// The keys of an effect: the size of one and its layout by its kind
// (FUN_0041fab5, and the readers FUN_00420740, FUN_0041e080, FUN_0041dfd0,
// FUN_0041df60, FUN_00420870)
void readKeys(const std::vector<uint8_t>& d, size_t at, int kind, uint32_t count, std::vector<ParticleKey>& out)
{
    for (uint32_t k = 0; k < count && k < 256; k++) {
        ParticleKey key;
        switch (kind) {
        case 0: case 1: case 2: case 3: {  // three floats and an int moment
            const size_t q = at + 16 * size_t(k);
            for (int c = 0; c < 3; c++)
                key.value[c] = readFloat(d, q + 4 * size_t(c));
            key.time = int(read32(d, q + 12));
            break;
        }
        case 6: case 11: {  // three shorts and a short moment
            const size_t q = at + 8 * size_t(k);
            for (int c = 0; c < 3; c++)
                key.value[c] = float(read16(d, q + 2 * size_t(c)));
            key.time = read16(d, q + 6);
            break;
        }
        default: {  // a short and a short moment
            const size_t q = at + 4 * size_t(k);
            key.value[0] = float(read16(d, q));
            key.time = read16(d, q + 2);
            break;
        }
        }
        out.push_back(key);
    }
}

float chance()
{
    return float(std::rand()) / float(RAND_MAX);
}

// The keys in force over a stretch of a particle's life, piece by piece
// (FUN_00420740, FUN_0041e080: the key before each moment holds until the
// next one): calls use(value, milliseconds) for each piece
template <typename Use>
void overStretch(const std::vector<ParticleKey>& keys, int age, int milliseconds, Use use)
{
    if (keys.empty())
        return;
    while (milliseconds > 0) {
        size_t k = 1;
        while (k < keys.size() && keys[k].time <= age)
            k++;
        int piece = milliseconds;
        if (k < keys.size() && keys[k].time < age + milliseconds)
            piece = keys[k].time - age;
        if (piece <= 0)
            piece = milliseconds;
        use(keys[k - 1], piece);
        age += piece;
        milliseconds -= piece;
    }
}

// The key set right at a moment of the stretch (FUN_0041df60, FUN_00420870),
// nullptr if none
const ParticleKey* keyIn(const std::vector<ParticleKey>& keys, int age, int milliseconds)
{
    for (size_t k = keys.size(); k-- > 0;)
        if (keys[k].time == age || (age < keys[k].time && keys[k].time < age + milliseconds))
            return &keys[k];
    return nullptr;
}

} // namespace

bool loadParticleSystems(const std::string& dataFolder, int level, const std::string& fileName,
                         std::vector<ParticleSystem>& out)
{
    out.clear();
    const std::filesystem::path path = std::filesystem::path(dataFolder) / "levels" / std::to_string(level) / fileName;
    std::vector<uint8_t> d;
    if (!readDataFile(path.string(), d) || d.size() < 12 || std::memcmp(d.data(), "DRAM", 4) != 0)
        return false;
    // DRAM: size, number of pointers, block, pointers; then the G3DB chunk:
    // its slot, size and number of pointers, then its block
    const size_t chunk = 12 + size_t(read32(d, 4)) + 4 * size_t(read32(d, 8));
    if (chunk + 16 > d.size() || std::memcmp(&d[chunk], "G3DB", 4) != 0)
        return false;
    const size_t block = chunk + 16;
    const uint32_t count = read32(d, block + 0x14), table = read32(d, block + 0x18);
    for (uint32_t k = 0; k < count && k < 256; k++) {
        const size_t r = block + table + 0x60 * size_t(k);
        if (r + 0x60 > d.size())
            return false;
        ParticleSystem s;
        s.count = int(read32(d, r));
        s.life = int(read32(d, r + 4));
        s.interval = std::max(1, int(read32(d, r + 8)));
        s.onlyLiving = read32(d, r + 0x0C) != 0;
        s.attached = read32(d, r + 0x10) != 0;
        s.randomAngle = read32(d, r + 0x14) != 0;
        s.emitTime = int(read32(d, r + 0x18));
        s.size = float(int(read32(d, r + 0x1C)));
        s.division = int(int8_t(d[r + 0x20]));
        s.shape = ParticleShape(std::clamp(int(read32(d, r + 0x24)), 0, 2));
        decodeBankTexture(d, block, r + 0x28, s.texture);
        const uint32_t effects = read32(d, r + 0x58), list = read32(d, r + 0x5C);
        for (uint32_t e = 0; e < effects && e < 32; e++) {
            const size_t q = block + list + 12 * size_t(e);
            ParticleEffect effect;
            effect.kind = int(read32(d, q));
            readKeys(d, block + read32(d, q + 8), effect.kind, read32(d, q + 4), effect.keys);
            s.effects.push_back(std::move(effect));
        }
        s.count = std::clamp(s.count, 0, 4096);
        out.push_back(std::move(s));
    }
    return true;
}

ParticleEmitter::ParticleEmitter(const ParticleSystem* system) : system_(system)
{
    if (system_) {
        particles_.resize(size_t(system_->count));
        sinceShown_ = system_->emitTime;  // not letting out until shown
    }
}

bool ParticleEmitter::alive(const Particle& p) const
{
    return system_ && p.age >= 1 && p.age <= system_->life;  // FUN_0041f050
}

bool ParticleEmitter::drawn(const Particle& p) const
{
    return system_ && p.visible && (system_->onlyLiving ? alive(p) : p.age > 0);
}

void ParticleEmitter::stop()
{
    if (!system_)
        return;
    sinceShown_ = system_->emitTime;
    for (Particle& p : particles_)
        p = Particle();
}

// A new particle where the emitter is (FUN_0041e51a)
void ParticleEmitter::start(Particle& p)
{
    p = Particle();
    std::memcpy(p.place, place, sizeof(place));
    p.angle = system_->randomAngle ? chance() * 2 * kPi : -kPi / 4;
    p.size = system_->size;
}

// A particle lives some milliseconds: its effects, then its place, spin and
// age (FUN_0041fb29, FUN_0041e632)
void ParticleEmitter::live(Particle& p, int milliseconds)
{
    for (const ParticleEffect& e : system_->effects) {
        switch (e.kind) {
        case 0: case 1: case 2: case 3:
            overStretch(e.keys, p.age, milliseconds, [&](const ParticleKey& key, int piece) {
                float a[3] = { key.value[0], key.value[1], key.value[2] };
                if (e.kind == 1 || e.kind == 3)  // by chance, between minus and plus
                    for (float& c : a)
                        if (c > 0)
                            c = 2 * chance() * c - c;
                if (e.kind >= 2) {  // turned as the emitter
                    const float x = a[0], y = a[1];
                    a[0] = x * std::cos(turn) - y * std::sin(turn);
                    a[1] = x * std::sin(turn) + y * std::cos(turn);
                }
                for (int c = 0; c < 3; c++)
                    p.velocity[c] += float(piece) * a[c];
            });
            break;
        case 4: case 5:
            overStretch(e.keys, p.age, milliseconds, [&](const ParticleKey& key, int piece) {
                float v = key.value[0];
                if (e.kind == 5 && v > 0)
                    v = 2 * chance() * v - v;
                p.spin += v * float(piece);
            });
            break;
        case 6: {
            // Its colour at the end of the stretch, between two keys (FUN_0041dfd0)
            const int at = p.age + milliseconds;
            const ParticleKey* before = &e.keys.back();
            const ParticleKey* after = nullptr;
            for (size_t k = 1; k < e.keys.size(); k++)
                if (at < e.keys[k].time) {
                    before = &e.keys[k - 1];
                    after = &e.keys[k];
                    break;
                }
            for (int c = 0; c < 3; c++) {
                float v = before->value[c];
                if (after && after->time > before->time)
                    v += (after->value[c] - v) * float(at - before->time) / float(after->time - before->time);
                p.colour[c] = uint8_t(std::clamp(2.0f * v, 0.0f, 255.0f));
            }
            break;
        }
        case 7: case 10:
            overStretch(e.keys, p.age, milliseconds, [&](const ParticleKey& key, int piece) {
                float v = key.value[0];
                if (e.kind == 10 && v > 0)
                    v = float(std::rand() % (int(v) * 2 + 1)) - v;
                p.size += v * float(piece);
            });
            p.size = std::max(p.size, 0.0f);
            break;
        case 8:
            if (const ParticleKey* key = keyIn(e.keys, p.age, milliseconds))
                p.visible = key->value[0] != 0;
            break;
        case 9:
            if (const ParticleKey* key = keyIn(e.keys, p.age, milliseconds))
                p.blend = int(key->value[0]);
            break;
        case 11:
            if (const ParticleKey* key = keyIn(e.keys, p.age, milliseconds)) {
                const FlarePicture& t = system_->texture;
                if (t.width > 0 && t.height > 0) {
                    p.u0 = (key->value[0] + 0.5f) / float(t.width);
                    p.v0 = (key->value[1] + 0.5f) / float(t.height);
                    p.u1 = (key->value[0] + key->value[2] - 1.0f) / float(t.width);
                    p.v1 = (key->value[1] + key->value[2] - 1.0f) / float(t.height);
                }
            }
            break;
        default:
            break;
        }
    }
    if (system_->attached)
        std::memcpy(p.place, place, sizeof(place));
    else
        for (int c = 0; c < 3; c++)
            p.place[c] += float(milliseconds) * p.velocity[c];
    p.angle += p.spin * kPi * float(milliseconds) / 65536.0f / 4096.0f;
    p.age += milliseconds;
}

void ParticleEmitter::emit(int milliseconds)
{
    if (!system_ || milliseconds <= 0)
        return;
    while (owed_ < milliseconds) {
        const int left = milliseconds - owed_;
        for (Particle& p : particles_)
            if (!alive(p)) {
                start(p);
                live(p, left);
                break;
            }
        owed_ += system_->interval;
    }
    owed_ -= milliseconds;
}

void ParticleEmitter::step(int milliseconds)
{
    if (!system_ || milliseconds <= 0)
        return;
    for (Particle& p : particles_)
        if (alive(p))
            live(p, milliseconds);
    if (sinceShown_ < system_->emitTime) {
        emit(milliseconds);
        sinceShown_ += milliseconds;
    }
}
