#include "game/fx/particles.h"

#include <algorithm>
#include <cmath>

#include "game/world/world.h"

void Particles::reset() {
    emitters_.clear();
    particles_.clear();
    rng_ = 0x9e3779b9u;
}

float Particles::random() {
    rng_ ^= rng_ << 13; // xorshift32
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (float)(rng_ >> 8) / (float)(1u << 24);
}

bool Particles::playing(const ParticlesDef* def) const {
    for (const Emitter& e : emitters_)
        if (e.def == def) return true;
    for (const Particle& p : particles_)
        if (p.def == def) return true;
    return false;
}

void Particles::play(const World& world, const ParticlesDef& def) {
    if (!world.objects.activeInHierarchy(def.object) || playing(&def)) return;
    emitters_.push_back({&def, 0.0f});
}

void Particles::emit(const ParticlesDef& def, float x, float y, int count) {
    for (int i = 0; i < count; i++) {
        // Circle shape: anywhere inside it (uniform over the area), moving away from its center.
        const float angle = random() * 6.2831853f, r = def.shapeRadius * std::sqrt(random());
        const float dx = std::cos(angle), dy = std::sin(angle);
        const float speed = def.speedMin + (def.speedMax - def.speedMin) * random();
        const float lifetime = def.lifetimeMin + (def.lifetimeMax - def.lifetimeMin) * random();
        particles_.push_back({&def, x + dx * r, y + dy * r, dx * speed, dy * speed, 0.0f, lifetime});
    }
}

void Particles::update(const World& world, float dt) {
    // A system on an object turned off stops, its particles with it.
    auto off = [&](const ParticlesDef* def) { return !world.objects.activeInHierarchy(def->object); };
    emitters_.erase(std::remove_if(emitters_.begin(), emitters_.end(), [&](const Emitter& e) { return off(e.def); }),
                    emitters_.end());
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [&](const Particle& p) { return off(p.def); }),
                     particles_.end());

    // Particles move, slow down and age (the ones emitted below start next frame).
    for (Particle& p : particles_) {
        const ParticlesDef& d = *p.def;
        float speed = std::sqrt(p.vx * p.vx + p.vy * p.vy);
        if (d.limitVelocity && speed > 0) {
            float s = speed;
            if (d.drag > 0) {
                const float k = d.drag * (d.dragBySize ? d.size * d.size : 1.0f) * (d.dragByVelocity ? s * s : s);
                s = std::max(0.0f, s - k * dt);
            }
            if (s > d.limit) s = d.limit + (s - d.limit) * std::pow(1.0f - d.dampen, dt * 60.0f);
            p.vx *= s / speed;
            p.vy *= s / speed;
        }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.age += dt;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.age >= p.lifetime; }),
                     particles_.end());

    // Bursts due in [time, time + dt) (the first frame after Play fires time 0), within the duration.
    for (Emitter& e : emitters_) {
        const ParticlesDef& d = *e.def;
        const float from = e.time, to = e.time + dt;
        float x, y;
        world.objectPosition(d.object, x, y);
        for (const ParticleBurstDef& b : d.bursts)
            for (int k = 0; k < b.cycles; k++) {
                const float t = b.time + k * b.interval;
                if (t >= d.duration || t < from || t >= to) continue;
                const float n = b.countMin + (b.countMax - b.countMin) * random();
                emit(d, x, y, (int)std::lround(n));
            }
        e.time = to;
    }
    emitters_.erase(std::remove_if(emitters_.begin(), emitters_.end(), [](const Emitter& e) { return e.time >= e.def->duration; }),
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
    const uint32_t base = p.def->color;
    const float alpha = (float)(base & 0xff) * std::min(1.0f, std::max(0.0f, a));
    return (base & 0xffffff00u) | (uint32_t)std::lround(alpha);
}
