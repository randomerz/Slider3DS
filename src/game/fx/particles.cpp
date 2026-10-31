#include "game/fx/particles.h"

#include <algorithm>
#include <cmath>

#include "game/world/world.h"

void Particles::reset() {
    emitters_.clear();
    particles_.clear();
    wasActive_.clear();
    nextId_ = 1;
    rng_ = Rng();
}

bool Particles::playing(int def) const {
    for (const Emitter& e : emitters_)
        if (e.def == def && !e.spawned) return true;
    return false;
}

void Particles::start(const World& world, int def, int object, float dx, float dy, bool spawned, bool waiting) {
    const ParticlesDef& d = world.area.particles[def];
    Emitter e{};
    e.def = def;
    e.id = nextId_++;
    // (a start delay between two constants is a random pick; a fixed one draws nothing)
    e.time = -(d.delayMax > d.delayMin ? d.delayMin + (d.delayMax - d.delayMin) * random() : d.delayMin);
    e.emitting = true;
    e.spawned = spawned;
    e.object = object;
    e.dx = dx;
    e.dy = dy;
    e.waiting = waiting;
    emitters_.push_back(e);
}

void Particles::play(const World& world, int def) {
    const int object = world.area.particles[def].object;
    if (object < 0 || !world.objects.activeInHierarchy(object) || playing(def)) return;
    start(world, def, object, 0, 0, false);
}

void Particles::stop(int def) {
    emitters_.erase(std::remove_if(emitters_.begin(), emitters_.end(), [&](const Emitter& e) { return e.def == def && !e.spawned; }),
                    emitters_.end());
}

void Particles::spawn(const World& world, int def, float dx, float dy) {
    const int object = world.area.particles[def].object;
    if (object < 0 || !world.objects.activeInHierarchy(object)) return;
    start(world, def, object, dx, dy, true);
}

void Particles::spawnPrefab(const World& world, ParticleType type, float x, float y, int parent) {
    const int def = world.area.particlePrefabs[(int)type];
    if (def < 0) return;
    float px = 0, py = 0;
    if (parent >= 0) world.objectPosition(parent, px, py);
    start(world, def, parent, x - px, y - py, true, parent >= 0 && !world.objects.activeInHierarchy(parent));
}

void Particles::randomInUnitSphere(float& x, float& y) {
    float z;
    do { // (rejection: uniform in the ball)
        x = random() * 2 - 1;
        y = random() * 2 - 1;
        z = random() * 2 - 1;
    } while (x * x + y * y + z * z > 1);
}

void Particles::origin(const World& world, const Emitter& e, float& x, float& y) const {
    x = y = 0;
    if (e.object >= 0) world.objectPosition(e.object, x, y);
    x += e.dx;
    y += e.dy;
}

uint32_t Particles::startColor(const ParticlesDef& def) {
    const std::vector<uint32_t>& c = def.colors;
    if (c.size() == 1) return c[0];
    // A random point on the gradient (Random Color): the key at or after it (fixed), or a blend.
    const float t = random();
    const std::vector<float>& times = def.colorTimes;
    if (t <= times.front()) return c.front();
    for (size_t i = 1; i < c.size(); i++) {
        if (t > times[i]) continue;
        if (def.colorsFixed) return c[i];
        const float f = times[i] > times[i - 1] ? (t - times[i - 1]) / (times[i] - times[i - 1]) : 1.0f;
        uint32_t out = 0;
        for (int shift = 24; shift >= 0; shift -= 8) {
            const float a = (float)((c[i - 1] >> shift) & 0xff), b = (float)((c[i] >> shift) & 0xff);
            out |= (uint32_t)std::lround(a + (b - a) * f) << shift;
        }
        return out;
    }
    return c.back();
}

void Particles::emit(const World& world, Emitter& emitter, float x, float y, int count) {
    const ParticlesDef& def = world.area.particles[emitter.def];
    float ox = 0, oy = 0;
    if (emitter.object >= 0) world.objectPosition(emitter.object, ox, oy);
    emitter.live += count;
    for (int i = 0; i < count; i++) {
        // Circle shape: anywhere inside it (uniform over the area), moving away from its center; an
        // arc: from a random angle in it.
        const float angle = def.arcStart + random() * def.arcLength, r = def.shapeRadius * std::sqrt(random());
        const float dx = std::cos(angle), dy = std::sin(angle);
        const float speed = def.speedMin + (def.speedMax - def.speedMin) * random();
        const float lifetime = def.lifetimeMin + (def.lifetimeMax - def.lifetimeMin) * random();
        Particle p{&def, emitter.def, emitter.id, emitter.object, x + dx * r, y + dy * r, dx * speed, dy * speed, 0.0f, lifetime,
                   startColor(def)};
        if (!def.frames.empty()) { // (untextured ones draw no more randoms: their sequence stays as it was)
            p.rotation = def.rotationMin + (def.rotationMax - def.rotationMin) * random();
            p.spin = def.spinMin + (def.spinMax - def.spinMin) * random();
            p.row = std::min((int)(random() * (int)(def.frames.size() / def.tilesX)), (int)(def.frames.size() / def.tilesX) - 1);
            p.frameBlend = random();
        }
        p.objectX = ox;
        p.objectY = oy;
        p.centerDx = emitter.dx;
        p.centerDy = emitter.dy;
        particles_.push_back(p);
    }
}

void Particles::update(const World& world, float dt) {
    const Area& area = world.area;
    // A system on an object turned off stops, its particles with it.
    auto off = [&](int object) { return object >= 0 && !world.objects.activeInHierarchy(object); };
    emitters_.erase(std::remove_if(emitters_.begin(), emitters_.end(), [&](const Emitter& e) { return !e.waiting && off(e.object); }),
                    emitters_.end());
    for (Emitter& e : emitters_)
        if (e.waiting && !off(e.object)) e.waiting = false; // (its parent on: it plays from now)
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [&](const Particle& p) { return off(p.object); }),
                     particles_.end());

    // Play on awake: Unity's ParticleSystem.OnEnable plays it each time its object turns on.
    wasActive_.resize(area.particles.size(), 0);
    for (int i = 0; i < (int)area.particles.size(); i++) {
        if (area.particles[i].object < 0) continue; // (a prefab's)
        const bool on = !off(area.particles[i].object);
        if (area.particles[i].playOnAwake && on && !wasActive_[i]) play(world, i);
        wasActive_[i] = on ? 1 : 0;
    }

    // Bursts due in [time, time + dt) (the first frame after Play fires time 0), within the
    // duration (each loop, when looping); the rate's share, carried between frames.
    for (Emitter& e : emitters_) {
        if (!e.emitting || e.waiting) continue;
        const ParticlesDef& d = area.particles[e.def];
        const float from = e.time, to = e.time + dt;
        if (to <= 0) { // (still in its start delay)
            e.time = to;
            continue;
        }
        float x, y;
        origin(world, e, x, y);
        for (const ParticleBurstDef& b : d.bursts)
            for (int k = 0; k < b.cycles; k++) {
                const float t = b.time + k * b.interval;
                if (t >= d.duration) continue;
                // Its times: t, and with looping t + n * duration.
                float n = d.loop ? std::max(0.0f, std::ceil((from - t) / d.duration)) : 0.0f;
                for (float at = t + n * d.duration; at < to; at = t + (++n) * d.duration) {
                    // (a fractional count is truncated: measured, MiniSparkle's 2.5 emits 2 every time; a
                    // cycle that may not fire draws its chance first)
                    if (at >= from && (b.probability >= 1 || random() < b.probability))
                        emit(world, e, x, y, (int)(b.countMin + (b.countMax - b.countMin) * random()));
                    if (!d.loop) break;
                }
            }
        if (d.rateMax > 0) {
            const float start = std::max(from, 0.0f); // (from its delay's end)
            const float span = d.loop ? to - start : std::max(0.0f, std::min(to, d.duration) - start);
            e.rateDebt += (d.rateMin + (d.rateMax - d.rateMin) * random()) * span;
            const int n = (int)e.rateDebt;
            e.rateDebt -= (float)n;
            emit(world, e, x, y, n);
        }
        e.time = to;
        if (!d.loop && e.time >= d.duration) e.emitting = false;
    }

    // Particles (also the ones just emitted) are pushed by the force, slow down, move and age. Limit
    // velocity as measured in Unity (ParticleSystem.Simulate, the same at 30 and 60 fps): speed over
    // the limit loses the dampen fraction per 1/30 s, then drag: dv/dt = -k v^2 (dragByVelocity;
    // else -k v), k = drag times the particle's circle area pi (size/2)^2 (dragBySize), integrated
    // exactly per step.
    if (dt != stepDt_ || steps_.size() != area.particles.size()) { // (the game's step is fixed: once)
        stepDt_ = dt;
        steps_.resize(area.particles.size());
        for (size_t i = 0; i < area.particles.size(); i++) {
            const ParticlesDef& d = area.particles[i];
            steps_[i] = {std::pow(1.0f - d.dampen, dt * 30.0f), std::cos(d.orbital * dt), std::sin(d.orbital * dt)};
        }
    }
    int positionOf = -1; // (the object last looked up: nothing moves objects in this loop)
    float ox = 0, oy = 0;
    for (Particle& p : particles_) {
        const ParticlesDef& d = *p.def;
        const Step& step = steps_[p.defIndex];
        if (p.object >= 0 && p.object != positionOf && (d.localSpace || d.orbital != 0)) {
            world.objectPosition(p.object, ox, oy);
            positionOf = p.object;
        }
        if (d.localSpace && p.object >= 0) { // (local space: carried along by its object)
            p.x += ox - p.objectX;
            p.y += oy - p.objectY;
            p.objectX = ox;
            p.objectY = oy;
        }
        p.rotation += p.spin * dt;
        if (d.orbital != 0) { // (about the system's position: its position turns, then the velocity moves it)
            const float cx = p.centerDx + (p.object >= 0 ? ox : 0), cy = p.centerDy + (p.object >= 0 ? oy : 0);
            const float rx = p.x - cx, ry = p.y - cy;
            p.x = cx + step.orbitalCos * rx - step.orbitalSin * ry;
            p.y = cy + step.orbitalSin * rx + step.orbitalCos * ry;
        }
        p.vx += d.forceX * dt;
        p.vy += d.forceY * dt;
        const float speed = std::sqrt(p.vx * p.vx + p.vy * p.vy);
        if (d.limitVelocity && speed > 0) {
            float s = speed;
            if (s > d.limit) s = d.limit + (s - d.limit) * step.keep;
            if (d.drag > 0) {
                const float k = d.drag * (d.dragBySize ? 0.78539816f * d.size * d.size : 1.0f);
                s = d.dragByVelocity ? s / (1 + k * s * dt) : s * std::exp(-k * dt);
            }
            p.vx *= s / speed;
            p.vy *= s / speed;
        }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.age += dt;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                    [&](const Particle& p) {
                                        if (p.age < p.lifetime) return false;
                                        for (Emitter& e : emitters_) // (a stopped system's emitter is gone)
                                            if (e.id == p.emitter) e.live--;
                                        return true;
                                    }),
                     particles_.end());
    // A finished run counts as playing until its last particle dies.
    emitters_.erase(std::remove_if(emitters_.begin(), emitters_.end(), [](const Emitter& e) { return !e.emitting && e.live <= 0; }),
                    emitters_.end());
}

int Particles::frameSprite(const Particle& p) {
    const ParticlesDef& d = *p.def;
    if (d.frames.empty()) return -1;
    // Texture sheet animation (Grid, single row, once over the lifetime): frameOverTime, the
    // particle's blend of its two curves (their baked tables), times the frames in a row.
    const float t = p.lifetime > 0 ? std::min(p.age / p.lifetime, 1.0f) : 1.0f;
    const float at = t * ParticlesDef::FRAME_TABLE_STEPS;
    const int i = std::min((int)at, ParticlesDef::FRAME_TABLE_STEPS - 1);
    auto sample = [&](const std::vector<float>& table) { return table[i] + (table[i + 1] - table[i]) * (at - (float)i); };
    float v = sample(d.frameTableMin);
    if (!d.frameTableMax.empty()) v += (sample(d.frameTableMax) - v) * p.frameBlend;
    v *= d.frameMultiplier;
    const int frame = std::min(std::max((int)(v * d.tilesX), 0), d.tilesX - 1);
    return d.frames[p.row * d.tilesX + frame];
}

uint32_t Particles::color(const Particle& p) {
    // Color over lifetime: alpha keys, linear between them, held past the ends.
    const std::vector<std::pair<float, float>>& keys = p.def->alpha;
    const float t = p.lifetime > 0 ? p.age / p.lifetime : 1;
    float a = 1;
    if (!keys.empty()) {
        a = t <= keys.front().first ? keys.front().second : keys.back().second;
        for (size_t i = 1; i < keys.size(); i++)
            if (t >= keys[i - 1].first && t < keys[i].first) {
                const float f = (t - keys[i - 1].first) / (keys[i].first - keys[i - 1].first);
                a = keys[i - 1].second + (keys[i].second - keys[i - 1].second) * f;
                break;
            }
    }
    const uint32_t base = p.color;
    const float alpha = (float)(base & 0xff) * std::min(1.0f, std::max(0.0f, a));
    return (base & 0xffffff00u) | (uint32_t)std::lround(alpha);
}
