// ParticleSystem playback for the systems the export describes (Area::particles, ParticlesDef):
// bursts and a steady rate of flat squares in world space, emitted from a circle around the system's
// object, flying outward, pushed by a constant force, slowed by limit-velocity and faded by
// color-over-lifetime alpha. Systems with playOnAwake play whenever their object becomes active.
// Limit velocity follows speed curves measured in Unity (Slider3DSSceneSurvey.SurveyParticleSpeedBatch):
// within ~1% for SparkleEffect.
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"

class World;

class Particles {
public:
    struct Particle {
        const ParticlesDef* def;
        int defIndex;       // its Area::particles index
        int emitter;        // the emitter that made it (its id)
        float x, y, vx, vy; // world units, per second
        float age, lifetime;
        uint32_t color;     // start color (RGBA)
    };

    void reset(); // nothing playing, the random sequence from its start

    // ParticleSystem.Play on Area::particles[def]: starts emitting from the top unless it is playing
    // (emitting, or a finished run's particles still alive); nothing while its object is inactive.
    void play(const World& world, int def);
    // ParticleSystem.Stop: emission stops (a later Play starts over); live particles play out.
    void stop(int def);
    bool playing(int def) const;
    // Instantiate of a prefab that plays on awake: a copy of `def` that plays once on its own,
    // emitting at (dx, dy) from the def's object (and following it).
    void spawn(const World& world, int def, float dx, float dy);
    // Systems whose object became active and play on awake start; due bursts and the rate's share
    // emit at their objects' positions; particles move, slow down, age and die. A system whose
    // object turns inactive stops and its particles vanish (Unity clears them on SetActive(false)).
    void update(const World& world, float dt);

    const std::vector<Particle>& particles() const { return particles_; }
    // Its color now: the start color with the color-over-lifetime alpha applied.
    static uint32_t color(const Particle& p);

private:
    struct Emitter {
        int def;           // Area::particles index
        int id;
        float time;        // s since Play
        bool emitting;     // false: finished (non-looping), kept while its particles live
        bool spawned;      // a spawned copy (Play / Stop / playing() ignore it)
        float dx, dy;      // offset from the def's object
        float rateDebt;    // particles owed by the rate, carried between frames
        int live;          // its particles alive
    };
    void emit(const Area& area, Emitter& emitter, float x, float y, int count);
    uint32_t startColor(const ParticlesDef& def);
    float random(); // [0, 1)

    std::vector<Emitter> emitters_;
    std::vector<Particle> particles_;
    std::vector<uint8_t> wasActive_; // per Area::particles entry: its object was active last update
    std::vector<float> keep_;        // per Area::particles entry: this frame's dampen factor (scratch)
    int nextId_ = 1;
    uint32_t rng_ = 0x9e3779b9u;
};
