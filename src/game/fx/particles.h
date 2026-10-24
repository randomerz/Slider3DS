// ParticleSystem playback for the systems the export describes (Area::particles, ParticlesDef):
// bursts and a steady rate of flat squares (or a texture sheet's frames, rotated and spinning) in
// world space (or local: moving with their object), emitted from a circle around the system's object,
// flying outward, pushed by a constant force, slowed by limit-velocity and faded by color-over-lifetime
// alpha. Systems with playOnAwake play whenever their object becomes active; ParticleManager's prefabs
// (Area::particlePrefabs) are spawned under any object (spawnPrefab).
// Limit velocity follows speed curves measured in Unity (Slider3DSSceneSurvey.SurveyParticleSpeedBatch):
// within ~1% for SparkleEffect.
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"
#include "game/data/rng.h"

class World;

class Particles {
public:
    struct Particle {
        const ParticlesDef* def;
        int defIndex;       // its Area::particles index
        int emitter;        // the emitter that made it (its id)
        int object;         // the object it's under (-1 = none): it vanishes with it, and follows it (local space)
        float x, y, vx, vy; // world units, per second
        float age, lifetime;
        uint32_t color;     // start color (RGBA)
        // Textured only: rotation (radians, counter-clockwise) and spin, its sheet row, its frame
        // curve's blend (0 = frameCurveMin, 1 = frameCurveMax); the object's position last update.
        float rotation = 0, spin = 0, frameBlend = 0;
        int row = 0;
        float objectX = 0, objectY = 0;
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
    // ParticleManager.SpawnParticle(type, (x, y), parent): its prefab plays once at that world
    // position, under `parent` (-1 = none: it stays put) — following it, and gone when it turns off.
    // Under a parent that's off it waits, and plays once the parent turns on (Unity instantiates it
    // inactive; its play-on-awake runs then). Nothing if the area has no such prefab.
    void spawnPrefab(const World& world, ParticleType type, float x, float y, int parent);
    // Random.insideUnitSphere's x and y (from this system's random sequence).
    void randomInUnitSphere(float& x, float& y);
    // Systems whose object became active and play on awake start; due bursts and the rate's share
    // emit at their objects' positions; particles move, slow down, age and die. A system whose
    // object turns inactive stops and its particles vanish (Unity clears them on SetActive(false)).
    void update(const World& world, float dt);

    const std::vector<Particle>& particles() const { return particles_; }
    // Its color now: the start color with the color-over-lifetime alpha applied.
    static uint32_t color(const Particle& p);
    // Its texture sheet frame now (an Area::sprites index), -1 = untextured (a flat square).
    static int frameSprite(const Particle& p);

private:
    struct Emitter {
        int def;           // Area::particles index
        int id;
        float time;        // s since Play
        bool emitting;     // false: finished (non-looping), kept while its particles live
        bool spawned;      // a spawned copy (Play / Stop / playing() ignore it)
        int object;        // what it emits from: the def's object, or a spawned prefab's parent (-1 = none)
        float dx, dy;      // offset from that object (none: the world position)
        bool waiting;      // spawned under an inactive parent: starts when it turns on (if never: kept
                           // until the next reset, as Unity keeps the inactive instance)
        float rateDebt;    // particles owed by the rate, carried between frames
        int live;          // its particles alive
    };
    void start(int def, int object, float dx, float dy, bool spawned, bool waiting = false);
    void emit(const World& world, Emitter& emitter, float x, float y, int count);
    void origin(const World& world, const Emitter& e, float& x, float& y) const; // where it emits now
    uint32_t startColor(const ParticlesDef& def);
    float random() { return rng_.unit(); } // [0, 1)

    std::vector<Emitter> emitters_;
    std::vector<Particle> particles_;
    std::vector<uint8_t> wasActive_; // per Area::particles entry: its object was active last update
    std::vector<float> keep_;        // per Area::particles entry: this frame's dampen factor (scratch)
    int nextId_ = 1;
    Rng rng_;
};
