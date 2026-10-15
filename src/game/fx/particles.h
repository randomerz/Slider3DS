// ParticleSystem playback for the systems the export describes (ParticlesDef): bursts of flat
// squares in world space, emitted from a circle around the system's object, flying outward, slowed
// by limit-velocity and faded by color-over-lifetime alpha.
// Not exact: Unity's limit-velocity drag formula isn't documented; drag here is quadratic in speed
// (dragByVelocity) on the particle's square area (dragBySize), and dampen is per 1/60 s.
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"

class World;

class Particles {
public:
    struct Particle {
        const ParticlesDef* def;
        float x, y, vx, vy; // world units, per second
        float age, lifetime;
    };

    void reset(); // nothing playing, the random sequence from its start

    // ParticleSystem.Play: starts emitting from the top unless it is still playing (emitting, or
    // particles alive); nothing while its object is inactive.
    void play(const World& world, const ParticlesDef& def);
    // Due bursts fire at their objects' positions; particles move, slow down, age and die. A system
    // whose object turns inactive stops and its particles vanish (Unity clears them on SetActive(false)).
    void update(const World& world, float dt);

    const std::vector<Particle>& particles() const { return particles_; }
    // Its color now: the start color with the color-over-lifetime alpha applied.
    static uint32_t color(const Particle& p);

private:
    struct Emitter {
        const ParticlesDef* def;
        float time; // s since Play
    };
    bool playing(const ParticlesDef* def) const;
    void emit(const ParticlesDef& def, float x, float y, int count);
    float random(); // [0, 1)

    std::vector<Emitter> emitters_;
    std::vector<Particle> particles_;
    uint32_t rng_ = 0x9e3779b9u;
};
