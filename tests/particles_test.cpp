// Particles (ParticleSystem playback) with the Village's knot sparkles (SparkleEffect): bursts,
// motion slowed by limit-velocity, the fade, Play while playing, and stopping with the object.
#include <cmath>
#include <string>

#include "game/fx/particles.h"
#include "game/world/world.h"
#include "test.h"

namespace {

const ParticlesDef& sparkles() { return villageArea().knotBoxes.at(0).particles.at(0); }

} // namespace

TEST(particles_sparkle_export) {
    const ParticlesDef& d = sparkles();
    CHECK(d.duration == 3 && d.lifetimeMin == 2 && d.lifetimeMax == 2.5f && d.speedMin == 1 && d.speedMax == 4);
    CHECK(d.size == 0.0625f && d.shapeRadius == 0.1f && d.color == 0xe6e6e6ffu);
    check(d.bursts.size() == 1 && d.bursts[0].countMin == 3 && d.bursts[0].countMax == 3 && d.bursts[0].cycles == 3 &&
              std::fabs(d.bursts[0].interval - 0.05f) < 1e-6f,
          "3 bursts of 3, 0.05 s apart");
    CHECK(d.limitVelocity && d.limit == 0.05f && d.dampen == 0.15f);
    CHECK(d.alpha.size() == 3 && d.alpha[2].second == 0);
}

TEST(particles_burst_fly_out_slow_down_and_fade) {
    const Area& area = villageArea();
    World world(area);
    world.setTileActive(4, true);
    const ParticlesDef& d = sparkles();
    float ox, oy;
    world.objectPosition(d.object, ox, oy);
    world.particles.play(world, d);
    const float dt = 1.0f / 60;
    size_t counts[8];
    for (int f = 0; f < 8; f++) {
        world.particles.update(world, dt);
        counts[f] = world.particles.particles().size();
    }
    // Bursts at 0, 0.05 and 0.1 s (frame f covers [f/60, (f+1)/60); checked away from the boundaries).
    check(counts[0] == 3 && counts[1] == 3 && counts[4] == 6 && counts[7] == 9,
          "bursts: " + std::to_string(counts[0]) + " " + std::to_string(counts[4]) + " " + std::to_string(counts[7]));

    // They move away from the emitter, fast at first, then crawl (over the 0.05 limit, dampened).
    float maxDist = 0, maxSpeed = 0;
    for (int f = 0; f < 60; f++) world.particles.update(world, dt); // ~1 s
    for (const Particles::Particle& p : world.particles.particles()) {
        maxDist = std::max(maxDist, std::hypot(p.x - ox, p.y - oy));
        maxSpeed = std::max(maxSpeed, std::hypot(p.vx, p.vy));
        CHECK((Particles::color(p) & 0xff) == 0xff); // not fading yet
    }
    check(maxDist > 0.1f && maxDist < 1.0f, "spread " + std::to_string(maxDist));
    check(maxSpeed < 0.1f, "slowed to a crawl " + std::to_string(maxSpeed));

    // The last quarter of their life fades out; all are gone after 2.5 s.
    for (int f = 0; f < 50; f++) world.particles.update(world, dt); // ~1.85 s
    bool fading = false;
    for (const Particles::Particle& p : world.particles.particles()) fading |= (Particles::color(p) & 0xff) < 0xff;
    CHECK(fading);
    for (int f = 0; f < 60; f++) world.particles.update(world, dt);
    check(world.particles.particles().empty(), "all gone");
}

TEST(particles_play_while_playing_and_objects_turning_off) {
    const Area& area = villageArea();
    World world(area);
    world.setTileActive(4, true);
    const ParticlesDef& d = sparkles();
    const float dt = 1.0f / 60;
    world.particles.play(world, d);
    for (int f = 0; f < 30; f++) world.particles.update(world, dt);
    const size_t n = world.particles.particles().size();
    world.particles.play(world, d); // still playing: no restart
    for (int f = 0; f < 3; f++) world.particles.update(world, dt);
    check(world.particles.particles().size() == n, "no new burst");

    world.objects.setActive(area, d.object, false); // SetActive(false) clears the system
    world.particles.update(world, dt);
    CHECK(world.particles.particles().empty());
    world.particles.play(world, d); // and Play does nothing while it's off
    world.particles.update(world, dt);
    CHECK(world.particles.particles().empty());
}

TEST(particles_slow_down_like_unity) {
    // SparkleEffect's limit velocity with one particle, no spread: speed and distance against
    // Unity's (ParticleSystem.Simulate at 60 fps; Slider3DSSceneSurvey.SurveyParticleSpeedBatch).
    const Area& area = villageArea();
    World world(area);
    world.setTileActive(4, true);
    struct Sample { float t, v, x; };
    const struct { float speed; Sample at[3]; } runs[] = {
        {4, {{0.2f, 1.4319f, 0.4750f}, {0.5f, 0.3586f, 0.6952f}, {1.0f, 0.0765f, 0.7752f}}},
        {1, {{0.2f, 0.4009f, 0.1253f}, {0.5f, 0.1302f, 0.1930f}, {1.0f, 0.0569f, 0.2324f}}},
    };
    for (const auto& run : runs) {
        ParticlesDef d = sparkles();
        d.speedMin = d.speedMax = run.speed;
        d.lifetimeMin = d.lifetimeMax = d.duration = 10;
        d.shapeRadius = 0;
        d.bursts = {{0, 1, 1, 1, 0}};
        world.particles.reset();
        float ox, oy;
        world.objectPosition(d.object, ox, oy);
        world.particles.play(world, d);
        int frame = 0;
        for (const Sample& s : run.at) {
            while (frame < (int)std::lround(s.t * 60)) {
                world.particles.update(world, 1.0f / 60);
                frame++;
            }
            const Particles::Particle& p = world.particles.particles().at(0);
            const float v = std::hypot(p.vx, p.vy), x = std::hypot(p.x - ox, p.y - oy);
            check(std::fabs(v - s.v) < 0.02f * s.v + 1e-3f && std::fabs(x - s.x) < 0.02f * s.x,
                  "speed " + std::to_string(run.speed) + " at " + std::to_string(s.t) + " s: v " + std::to_string(v) +
                      " (Unity " + std::to_string(s.v) + "), x " + std::to_string(x) + " (Unity " + std::to_string(s.x) + ")");
        }
    }
}
