#pragma once

// The game's particle systems (objects of kind 7 of its renderer: rain,
// lightning in the sky, the light of the other hero far away, splashes...),
// read from the G3DB bank of the level's data (CarnbyNN, AlineNN: its
// +0x14 how many, +0x18 where, 0x60 bytes each; G3D_ResourceBank_UploadChunk)
// and run as the game (FUN_0041f0b7 makes an emitter of one, FUN_0041f480
// lets particles out, FUN_0041f5eb moves them, FUN_0041fb29 their effects,
// FUN_0041e632 their place, FUN_0041e6df draws them). docs/FORMATOS.md.

#include <cstdint>
#include <string>
#include <vector>

#include "handLight.h"

// A key of an effect: up to three values and its moment (milliseconds of the
// particle's life)
struct ParticleKey {
    float value[3] = {};
    int time = 0;
};

// An effect on the particles along their life (0x0C bytes: its kind, how many
// keys, where): 0 and 2 accelerate them (2 turned as the emitter), 1 and 3
// by chance; 4 and 5 make them spin (5 by chance); 6 their colour; 7 and 10
// their size (10 by chance); 8 shows or hides them; 9 their way of blending;
// 11 the part of the texture they show
struct ParticleEffect {
    int kind = 0;
    std::vector<ParticleKey> keys;
};

// How a system's particles are drawn (its +0x24)
enum class ParticleShape { Sprite = 0, Flat = 1, Streak = 2 };

struct ParticleSystem {
    int count = 0;        // particles at most (+0x00)
    int life = 0;         // milliseconds each lives (+0x04)
    int interval = 0;     // one let out every that many milliseconds (+0x08)
    bool onlyLiving = false;  // draws only those alive (+0x0C), else every one that lived
    bool attached = false;    // they stay where the emitter is (+0x10)
    bool randomAngle = false; // each starts turned by chance (+0x14)
    int emitTime = 0;     // it lets particles out that long after being shown (+0x18)
    float size = 0;       // of a new particle (+0x1C, 1/8192 of a unit for sprites)
    int division = 0;     // flat ones are cut into this many squares + 1 a side (+0x20)
    ParticleShape shape = ParticleShape::Sprite;
    FlarePicture texture;  // its record at +0x28
    std::vector<ParticleEffect> effects;  // +0x58 how many, +0x5C where
};

// Reads the systems of the G3DB bank of a level's data file
// ("levels\\30\\Carnby01"). False if it cannot be read (none is no error)
bool loadParticleSystems(const std::string& dataFolder, int level, const std::string& fileName,
                         std::vector<ParticleSystem>& out);

// A particle (0xC4 bytes in the game)
struct Particle {
    float velocity[3] = {};  // units a millisecond
    float place[3] = {};
    float spin = 0;          // keys' units
    float angle = 0;         // radians
    float size = 0;
    int age = 0;             // milliseconds; alive from 1 to the system's life
    uint8_t colour[3] = { 255, 255, 255 };
    int blend = -1;          // the game's way of blending (-1: the first)
    bool visible = true;
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;  // the part of the texture
};

// An emitter of a system, somewhere in the world
class ParticleEmitter {
public:
    explicit ParticleEmitter(const ParticleSystem* system = nullptr);
    const ParticleSystem* system() const { return system_; }
    float place[3] = {};
    float turn = 0;  // radians about z (the effects of kind 2)
    // Shown this frame (FUN_0041f41a): it goes on letting particles out
    void keepEmitting() { sinceShown_ = 0; }
    // Lets particles out for some milliseconds (FUN_0041f480)
    void emit(int milliseconds);
    // A frame: the particles move and live; it lets more out while shown
    // (FUN_0041f5eb)
    void step(int milliseconds);
    // Stops letting out at once and forgets every particle (FUN_0041f42f)
    void stop();
    const std::vector<Particle>& particles() const { return particles_; }
    std::vector<Particle>& particles() { return particles_; }
    // Whether a particle is to be drawn (FUN_0041f6d4)
    bool drawn(const Particle& p) const;
    bool alive(const Particle& p) const;

private:
    void start(Particle& p);
    void live(Particle& p, int milliseconds);

    const ParticleSystem* system_ = nullptr;
    std::vector<Particle> particles_;
    int sinceShown_ = 0;  // +0x1C; lets out while under the system's emitTime
    int owed_ = 0;        // +0x13C
};
