// Particles (ParticleSystem playback) with the Village's systems: the knot sparkles (SparkleEffect:
// bursts, motion slowed by limit-velocity, the fade, Play while playing, stopping with the object),
// the ruins' looping sparkles (rate, force, Stop), play on awake, spawned copies and random colors.
#include <cmath>
#include <string>

#include "game/fx/particles.h"
#include "game/world/world.h"
#include "test.h"

namespace {

int sparkles() { return villageArea().knotBoxes.at(0).particles.at(0); } // Area::particles index

// The live particles of Area::particles[def].
int countOf(const World& world, int def) {
    int n = 0;
    for (const Particles::Particle& p : world.particles.particles()) n += p.def == &world.area.particles[def];
    return n;
}

int particlesOn(const Area& area, const std::string& pathEnd) {
    for (int i = 0; i < (int)area.particles.size(); i++) {
        const std::string& path = area.pathOf(area.particles[i].object);
        if (path.size() >= pathEnd.size() && path.compare(path.size() - pathEnd.size(), pathEnd.size(), pathEnd) == 0) return i;
    }
    return -1;
}

} // namespace

TEST(particles_sparkle_export) {
    const ParticlesDef& d = villageArea().particles.at(sparkles());
    CHECK(d.duration == 3 && d.lifetimeMin == 2 && d.lifetimeMax == 2.5f && d.speedMin == 1 && d.speedMax == 4);
    CHECK(d.size == 0.0625f && d.shapeRadius == 0.1f && d.colors.size() == 1 && d.colors[0] == 0xe6e6e6ffu);
    CHECK(!d.loop && !d.playOnAwake && d.rateMax == 0 && d.forceY == 0);
    check(d.bursts.size() == 1 && d.bursts[0].countMin == 3 && d.bursts[0].countMax == 3 && d.bursts[0].cycles == 3 &&
              std::fabs(d.bursts[0].interval - 0.05f) < 1e-6f,
          "3 bursts of 3, 0.05 s apart");
    CHECK(d.limitVelocity && d.limit == 0.05f && d.dampen == 0.15f);
    CHECK(d.alpha.size() == 3 && d.alpha[2].second == 0);
}

TEST(particles_play_on_awake_export) {
    // Every collectible's sparkles play on awake (looping, a steady rate, drifting up); systems the
    // port can't show (smoke, fish, waves: texture sheets) are left out.
    const Area& area = villageArea();
    const int s3 = particlesOn(area, "Slider 3 Collectible/Slider Sprite/SparklesConstant Variant");
    CHECK(s3 >= 0);
    const ParticlesDef& d = area.particles[s3];
    CHECK(d.playOnAwake && d.loop && d.rateMin == 2 && d.rateMax == 3 && d.forceY == 1 && d.bursts.empty());
    CHECK(particlesOn(area, "SmokeChimney") < 0 && particlesOn(area, "FishParticle") < 0);
    int onAwake = 0;
    for (const ParticlesDef& p : area.particles) onAwake += p.playOnAwake;
    CHECK(onAwake == 15);
}

TEST(particles_burst_fly_out_slow_down_and_fade) {
    const Area& area = villageArea();
    World world(area);
    world.setTileActive(4, true);
    const int k = sparkles();
    const ParticlesDef& d = area.particles[k];
    float ox, oy;
    world.objectPosition(d.object, ox, oy);
    world.particles.play(world, k);
    const float dt = 1.0f / 60;
    int counts[8];
    for (int f = 0; f < 8; f++) {
        world.particles.update(world, dt);
        counts[f] = countOf(world, k);
    }
    // Bursts at 0, 0.05 and 0.1 s (frame f covers [f/60, (f+1)/60); checked away from the boundaries).
    check(counts[0] == 3 && counts[1] == 3 && counts[4] == 6 && counts[7] == 9,
          "bursts: " + std::to_string(counts[0]) + " " + std::to_string(counts[4]) + " " + std::to_string(counts[7]));

    // They move away from the emitter, fast at first, then crawl (over the 0.05 limit, dampened).
    float maxDist = 0, maxSpeed = 0;
    for (int f = 0; f < 60; f++) world.particles.update(world, dt); // ~1 s
    for (const Particles::Particle& p : world.particles.particles()) {
        if (p.def != &d) continue;
        maxDist = std::max(maxDist, std::hypot(p.x - ox, p.y - oy));
        maxSpeed = std::max(maxSpeed, std::hypot(p.vx, p.vy));
        CHECK((Particles::color(p) & 0xff) == 0xff); // not fading yet
    }
    check(maxDist > 0.1f && maxDist < 1.0f, "spread " + std::to_string(maxDist));
    check(maxSpeed < 0.1f, "slowed to a crawl " + std::to_string(maxSpeed));

    // The last quarter of their life fades out; all are gone after 2.5 s.
    for (int f = 0; f < 50; f++) world.particles.update(world, dt); // ~1.85 s
    bool fading = false;
    for (const Particles::Particle& p : world.particles.particles())
        if (p.def == &d) fading |= (Particles::color(p) & 0xff) < 0xff;
    CHECK(fading);
    for (int f = 0; f < 60; f++) world.particles.update(world, dt);
    check(countOf(world, k) == 0, "all gone");
    CHECK(world.particles.playing(k)); // (until its 3 s duration ends)
    for (int f = 0; f < 10; f++) world.particles.update(world, dt);
    CHECK(!world.particles.playing(k)); // then done: a Play would start over
}

TEST(particles_play_while_playing_and_objects_turning_off) {
    const Area& area = villageArea();
    World world(area);
    world.setTileActive(4, true);
    const int k = sparkles();
    const float dt = 1.0f / 60;
    world.particles.play(world, k);
    for (int f = 0; f < 30; f++) world.particles.update(world, dt);
    const int n = countOf(world, k);
    world.particles.play(world, k); // still playing: no restart
    for (int f = 0; f < 3; f++) world.particles.update(world, dt);
    check(countOf(world, k) == n, "no new burst");

    world.objects.setActive(area, area.particles[k].object, false); // SetActive(false) clears the system
    world.particles.update(world, dt);
    CHECK(countOf(world, k) == 0);
    world.particles.play(world, k); // and Play does nothing while it's off
    world.particles.update(world, dt);
    CHECK(countOf(world, k) == 0);
}

TEST(particles_loop_rate_force_and_stop) {
    // A ruins rod's sparkles: a burst of 3 on Play, then 0.5-1.5 a second, pushed up by a force;
    // Stop ends the emission but lets them live; Play then starts over (a new burst).
    const Area& area = villageArea();
    World world(area);
    const int k = area.ruinsArrows.at(0).rods.at(0).particles;
    const ParticlesDef& d = area.particles[k];
    CHECK(d.loop && d.rateMin == 0.5f && d.rateMax == 1.5f && d.forceY == 0.5f);
    world.particles.play(world, k);
    const float dt = 1.0f / 60;
    world.particles.update(world, dt);
    CHECK(countOf(world, k) == 3);
    int emitted = 3;
    int last = 3;
    int old = 0, rising = 0;
    for (int f = 0; f < 600; f++) { // 10 s: about 10 more (each lives 2-2.5 s)
        world.particles.update(world, dt);
        const int now = countOf(world, k);
        if (now > last) emitted += now - last;
        last = now;
        for (const Particles::Particle& p : world.particles.particles())
            if (p.def == &d && p.age > 1.5f) old++, rising += p.vy > 0;
    }
    check(emitted >= 3 + 6 && emitted <= 3 + 14, "rate: " + std::to_string(emitted - 3) + " in 10 s");
    check(old > 0 && rising == old, "the force turns them all upward: " + std::to_string(rising) + "/" + std::to_string(old));
    CHECK(world.particles.playing(k));

    world.particles.stop(k);
    const int alive = countOf(world, k);
    CHECK(!world.particles.playing(k) && alive > 0);
    for (int f = 0; f < 60; f++) world.particles.update(world, dt);
    check(countOf(world, k) < alive || alive == 0, "no new ones after Stop");
    for (int f = 0; f < 120; f++) world.particles.update(world, dt);
    CHECK(countOf(world, k) == 0);
    world.particles.play(world, k);
    world.particles.update(world, dt);
    CHECK(countOf(world, k) == 3); // the burst again
}

TEST(particles_play_on_awake_when_the_object_turns_on) {
    // The buried Slider's sparkles play when it appears, and again each time it turns on.
    const Area& area = villageArea();
    World world(area);
    const int k = particlesOn(area, "Burried Slider/RuinsSparklesConstant Variant");
    CHECK(k >= 0 && area.particles[k].playOnAwake);
    world.setTileActive(5, true);
    const float dt = 1.0f / 60;
    for (int f = 0; f < 10; f++) world.particles.update(world, dt);
    CHECK(countOf(world, k) == 0 && !world.particles.playing(k)); // (buried: its object is off)
    const int buried = area.ruinsArrows.at(0).buriedSlider;
    world.objects.setActive(area, buried, true);
    world.particles.update(world, dt);
    check(countOf(world, k) == 10 && world.particles.playing(k), "a burst of 10 on awake: " + std::to_string(countOf(world, k)));
    world.objects.setActive(area, buried, false);
    world.particles.update(world, dt);
    CHECK(countOf(world, k) == 0 && !world.particles.playing(k));
    world.objects.setActive(area, buried, true);
    world.particles.update(world, dt);
    CHECK(countOf(world, k) == 10);
}

TEST(particles_spawned_copies_and_random_colors) {
    // The ruins trail's sparkle prefab, spawned twice at offsets from the arrow: each plays its bursts
    // (3 x 2) there on its own, in one of its gradient's two colors (fixed keys: gray or brown).
    const Area& area = villageArea();
    World world(area);
    world.setTileActive(2, true);
    const RuinsArrowDef& r = area.ruinsArrows.at(0);
    const int k = r.trail.particles;
    const ParticlesDef& d = area.particles[k];
    CHECK(!d.playOnAwake && !d.loop && d.colorsFixed && d.colors.size() == 3);
    CHECK(d.colors[0] == 0xe6e6e6ffu && d.colors[2] == 0xb08060ffu);
    world.particles.spawn(world, k, 3, 0);
    world.particles.spawn(world, k, -3, 0);
    CHECK(!world.particles.playing(k)); // (copies don't count for Play)
    const float dt = 1.0f / 60;
    for (int f = 0; f < 10; f++) world.particles.update(world, dt);
    check(countOf(world, k) == 12, "two copies' bursts: " + std::to_string(countOf(world, k)));
    float ax, ay;
    world.objectPosition(d.object, ax, ay);
    int left = 0, right = 0, gray = 0, brown = 0;
    for (const Particles::Particle& p : world.particles.particles()) {
        if (p.def != &d) continue;
        (p.x > ax ? right : left)++;
        gray += p.color == 0xe6e6e6ffu;
        brown += p.color == 0xb08060ffu;
    }
    CHECK(left == 6 && right == 6 && gray + brown == 12);
}

TEST(particles_slow_down_like_unity) {
    // SparkleEffect's limit velocity with one particle, no spread: speed and distance against
    // Unity's (ParticleSystem.Simulate at 60 fps; Slider3DSSceneSurvey.SurveyParticleSpeedBatch).
    struct Sample { float t, v, x; };
    const struct { float speed; Sample at[3]; } runs[] = {
        {4, {{0.2f, 1.4319f, 0.4750f}, {0.5f, 0.3586f, 0.6952f}, {1.0f, 0.0765f, 0.7752f}}},
        {1, {{0.2f, 0.4009f, 0.1253f}, {0.5f, 0.1302f, 0.1930f}, {1.0f, 0.0569f, 0.2324f}}},
    };
    for (const auto& run : runs) {
        Area area = villageArea(); // (a copy: the system changed for the measurement)
        const int k = sparkles();
        ParticlesDef& d = area.particles[k];
        d.speedMin = d.speedMax = run.speed;
        d.lifetimeMin = d.lifetimeMax = d.duration = 10;
        d.shapeRadius = 0;
        d.bursts = {{0, 1, 1, 1, 0}};
        World world(area);
        world.setTileActive(4, true);
        world.particles.reset();
        float ox, oy;
        world.objectPosition(d.object, ox, oy);
        world.particles.play(world, k);
        int frame = 0;
        for (const Sample& s : run.at) {
            while (frame < (int)std::lround(s.t * 60)) {
                world.particles.update(world, 1.0f / 60);
                frame++;
            }
            const Particles::Particle* p = nullptr;
            for (const Particles::Particle& q : world.particles.particles())
                if (q.def == &d) p = &q;
            CHECK(p != nullptr);
            if (!p) break;
            const float v = std::hypot(p->vx, p->vy), x = std::hypot(p->x - ox, p->y - oy);
            check(std::fabs(v - s.v) < 0.02f * s.v + 1e-3f && std::fabs(x - s.x) < 0.02f * s.x,
                  "speed " + std::to_string(run.speed) + " at " + std::to_string(s.t) + " s: v " + std::to_string(v) +
                      " (Unity " + std::to_string(s.v) + "), x " + std::to_string(x) + " (Unity " + std::to_string(s.x) + ")");
        }
    }
}
