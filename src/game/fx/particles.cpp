#include "game/fx/particles.h"

#include <algorithm>
#include <cmath>

#include "game/world/world.h"

void Particles::reset() {
    emitters_.clear();
    particles_.clear();
    wasActive_.clear();
    nextId_ = 1;
    rng_ = 0x9e3779b9u;
}

float Particles::random() {
    rng_ ^= rng_ << 13; // xorshift32
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (float)(rng_ >> 8) / (float)(1u << 24);
}

bool Particles::playing(int def) const {
    for (const Emitter& e : emitters_)
        if (e.def == def && !e.spawned) return true;
    return false;
}

void Particles::play(const World& world, int def) {
    if (!world.objects.activeInHierarchy(world.area.particles[def].object) || playing(def)) return;
    emitters_.push_back({def, nextId_++, 0.0f, true, false, 0.0f, 0.0f, 0.0f, 0});
}

void Particles::stop(int def) {
    emitters_.erase(std::remove_if(emitters_.begin(), emitters_.end(), [&](const Emitter& e) { return e.def == def && !e.spawned; }),
                    emitters_.end());
}

void Particles::spawn(const World& world, int def, float dx, float dy) {
    if (!world.objects.activeInHierarchy(world.area.particles[def].object)) return;
    emitters_.push_back({def, nextId_++, 0.0f, true, true, dx, dy, 0.0f, 0});
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

void Particles::emit(const Area& area, Emitter& emitter, float x, float y, int count) {
    const ParticlesDef& def = area.particles[emitter.def];
    emitter.live += count;
    for (int i = 0; i < count; i++) {
        // Circle shape: anywhere inside it (uniform over the area), moving away from its center.
        const float angle = random() * 6.2831853f, r = def.shapeRadius * std::sqrt(random());
        const float dx = std::cos(angle), dy = std::sin(angle);
        const float speed = def.speedMin + (def.speedMax - def.speedMin) * random();
        const float lifetime = def.lifetimeMin + (def.lifetimeMax - def.lifetimeMin) * random();
        particles_.push_back({&def, emitter.def, emitter.id, x + dx * r, y + dy * r, dx * speed, dy * speed, 0.0f, lifetime, startColor(def)});
    }
}

void Particles::update(const World& world, float dt) {
    const Area& area = world.area;
    // A system on an object turned off stops, its particles with it.
    auto off = [&](const ParticlesDef& def) { return !world.objects.activeInHierarchy(def.object); };
    emitters_.erase(std::remove_if(emitters_.begin(), emitters_.end(), [&](const Emitter& e) { return off(area.particles[e.def]); }),
                    emitters_.end());
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [&](const Particle& p) { return off(*p.def); }),
                     particles_.end());

    // Play on awake: Unity's ParticleSystem.OnEnable plays it each time its object turns on.
    wasActive_.resize(area.particles.size(), 0);
    for (int i = 0; i < (int)area.particles.size(); i++) {
        const bool on = !off(area.particles[i]);
        if (area.particles[i].playOnAwake && on && !wasActive_[i]) play(world, i);
        wasActive_[i] = on ? 1 : 0;
    }

    // Bursts due in [time, time + dt) (the first frame after Play fires time 0), within the
    // duration (each loop, when looping); the rate's share, carried between frames.
    for (Emitter& e : emitters_) {
        if (!e.emitting) continue;
        const ParticlesDef& d = area.particles[e.def];
        const float from = e.time, to = e.time + dt;
        float x, y;
        world.objectPosition(d.object, x, y);
        x += e.dx;
        y += e.dy;
        for (const ParticleBurstDef& b : d.bursts)
            for (int k = 0; k < b.cycles; k++) {
                const float t = b.time + k * b.interval;
                if (t >= d.duration) continue;
                // Its times: t, and with looping t + n * duration.
                float n = d.loop ? std::max(0.0f, std::ceil((from - t) / d.duration)) : 0.0f;
                for (float at = t + n * d.duration; at < to; at = t + (++n) * d.duration) {
                    if (at >= from) emit(area, e, x, y, (int)std::lround(b.countMin + (b.countMax - b.countMin) * random()));
                    if (!d.loop) break;
                }
            }
        if (d.rateMax > 0) {
            const float span = d.loop ? dt : std::max(0.0f, std::min(to, d.duration) - from);
            e.rateDebt += (d.rateMin + (d.rateMax - d.rateMin) * random()) * span;
            const int n = (int)e.rateDebt;
            e.rateDebt -= (float)n;
            emit(area, e, x, y, n);
        }
        e.time = to;
        if (!d.loop && e.time >= d.duration) e.emitting = false;
    }

    // Particles (also the ones just emitted) are pushed by the force, slow down, move and age. Limit
    // velocity as measured in Unity (ParticleSystem.Simulate, the same at 30 and 60 fps): speed over
    // the limit loses the dampen fraction per 1/30 s, then drag: dv/dt = -k v^2 (dragByVelocity;
    // else -k v), k = drag times the particle's circle area pi (size/2)^2 (dragBySize), integrated
    // exactly per step.
    keep_.resize(area.particles.size());
    for (size_t i = 0; i < area.particles.size(); i++) keep_[i] = std::pow(1.0f - area.particles[i].dampen, dt * 30.0f);
    for (Particle& p : particles_) {
        const ParticlesDef& d = *p.def;
        p.vx += d.forceX * dt;
        p.vy += d.forceY * dt;
        const float speed = std::sqrt(p.vx * p.vx + p.vy * p.vy);
        if (d.limitVelocity && speed > 0) {
            float s = speed;
            if (s > d.limit) s = d.limit + (s - d.limit) * keep_[p.defIndex];
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
