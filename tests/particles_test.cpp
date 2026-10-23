// Particles (ParticleSystem playback) with the Village's systems: the knot sparkles (SparkleEffect:
// bursts, motion slowed by limit-velocity, the fade, Play while playing, stopping with the object),
// the ruins' looping sparkles (rate, force, Stop), play on awake, spawned copies and random colors;
// ParticleManager's prefabs (SmokePoof: a texture sheet, local space; MiniSparkle) and where the
// Village spawns them.
#include <cmath>
#include <string>

#include "game/fx/particles.h"
#include "game/village/village_grid.h"
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
        if (area.particles[i].object < 0) continue; // (a prefab's)
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
    World world(area, DEFAULT_SETTINGS);
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
    World world(area, DEFAULT_SETTINGS);
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
    World world(area, DEFAULT_SETTINGS);
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
    World world(area, DEFAULT_SETTINGS);
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
    World world(area, DEFAULT_SETTINGS);
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
        World world(area, DEFAULT_SETTINGS);
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

TEST(particles_rate_between_two_constants) {
    // A ruins rod's sparkles without their burst: 0.5-1.5 a second. Unity (SurveyParticleSemanticsBatch
    // (c), 8 seeds) emitted 9-10 in 10 s, 0-2 in any one second: about the mean, with some jitter.
    Area area = villageArea();
    const int k = area.ruinsArrows.at(0).rods.at(0).particles;
    ParticlesDef& d = area.particles[k];
    d.bursts.clear();
    d.lifetimeMin = d.lifetimeMax = 100;
    World world(area, DEFAULT_SETTINGS);
    world.particles.reset();
    world.particles.play(world, k);
    int last = 0;
    for (int s = 0; s < 10; s++) {
        for (int f = 0; f < 60; f++) world.particles.update(world, 1.0f / 60);
        const int n = countOf(world, k);
        check(n - last <= 2, "second " + std::to_string(s) + ": " + std::to_string(n - last));
        last = n;
    }
    check(last >= 9 && last <= 11, "10 s: " + std::to_string(last));
}

TEST(particles_play_until_the_last_particle_dies) {
    // A non-looping system whose particles outlive its duration counts as playing (a Play does
    // nothing) until the last of them is gone.
    Area area = villageArea();
    const int k = sparkles();
    ParticlesDef& d = area.particles[k];
    CHECK(!d.loop);
    d.lifetimeMin = d.lifetimeMax = d.duration + 1;
    World world(area, DEFAULT_SETTINGS);
    world.setTileActive(4, true);
    world.particles.reset();
    world.particles.play(world, k);
    const float dt = 1.0f / 60;
    int f = 0, after = 0, mismatches = 0;
    for (; f * dt < d.duration + 0.05f; f++) world.particles.update(world, dt);
    for (; f * dt < 2 * d.duration + 2; f++) { // past the duration: playing while any are left
        world.particles.update(world, dt);
        after += countOf(world, k) > 0;
        mismatches += world.particles.playing(k) != (countOf(world, k) > 0);
    }
    check(after > 10 && mismatches == 0, std::to_string(after) + " frames with particles, " + std::to_string(mismatches) + " mismatches");
    CHECK(countOf(world, k) == 0 && !world.particles.playing(k));
}

TEST(particles_force_then_limit_like_unity) {
    // A ruins rod's sparkle from rest: the force (0.5 up) is added before limit velocity, as measured
    // in Unity (Slider3DSSceneSurvey.SurveyParticleSemanticsBatch (b); the other order is ~2% off).
    // The same survey measured that Stop + Play in one frame restarts with the burst (as the port
    // does); the rate's counts are pinned by particles_rate_between_two_constants.
    Area area = villageArea();
    const int k = area.ruinsArrows.at(0).rods.at(0).particles;
    ParticlesDef& d = area.particles[k];
    d.speedMin = d.speedMax = 0;
    d.lifetimeMin = d.lifetimeMax = 10;
    d.shapeRadius = 0;
    d.rateMin = d.rateMax = 0;
    d.bursts = {{0, 1, 1, 1, 0}};
    World world(area, DEFAULT_SETTINGS);
    world.particles.reset();
    float ox, oy;
    world.objectPosition(d.object, ox, oy);
    world.particles.play(world, k);
    const struct { int frame; float vy, y; } at[] = {{12, 0.09563f, 0.01063f}, {60, 0.28568f, 0.18067f}, {120, 0.34222f, 0.50256f}};
    int frame = 0;
    for (const auto& s : at) {
        while (frame < s.frame) {
            world.particles.update(world, 1.0f / 60);
            frame++;
        }
        const Particles::Particle* p = nullptr;
        for (const Particles::Particle& q : world.particles.particles())
            if (q.def == &d) p = &q;
        CHECK(p != nullptr);
        if (!p) break;
        check(std::fabs(p->vy - s.vy) < 0.003f * s.vy && std::fabs(p->y - oy - s.y) < 0.003f * s.y,
              "frame " + std::to_string(s.frame) + ": vy " + std::to_string(p->vy) + " (Unity " + std::to_string(s.vy) +
                  "), y " + std::to_string(p->y - oy) + " (Unity " + std::to_string(s.y) + ")");
    }
}

namespace {

int prefab(ParticleType t) { return villageArea().particlePrefabs[(int)t]; }

// Live particles of Area::particles[def] and their mean position.
int meanOf(const World& world, int def, float& x, float& y) {
    int n = 0;
    x = y = 0;
    for (const Particles::Particle& p : world.particles.particles())
        if (p.defIndex == def) n++, x += p.x, y += p.y;
    if (n) x /= n, y /= n;
    return n;
}

} // namespace

TEST(particles_prefabs_export) {
    // SmokePoof: 8 px frames of the smoke sheet (5 rows of 5), local space, turned and spinning, tinted
    // by its material (#e6e6e6); MiniSparkle: 1 px squares of #e6e6e6 (its one white sprite).
    const Area& area = villageArea();
    const int smoke = prefab(ParticleType::SmokePoof), sparkle = prefab(ParticleType::MiniSparkle);
    CHECK(smoke >= 0 && sparkle >= 0);
    if (smoke < 0 || sparkle < 0) return;
    const ParticlesDef& s = area.particles[smoke];
    CHECK(s.object == -1 && !s.playOnAwake && s.localSpace && s.size == 0.5f && s.colors[0] == 0xe6e6e6ffu);
    CHECK(s.frames.size() == 25 && s.tilesX == 5 && s.rotationMax > 6.28f && s.spinMin < 0 && s.spinMax > 0);
    CHECK(area.sprites[s.frames[0]].w == 8 && area.sprites[s.frames[0]].h == 8);
    CHECK(s.bursts.size() == 1 && s.bursts[0].cycles == 3 && s.bursts[0].countMax == 2);
    const ParticlesDef& m = area.particles[sparkle];
    CHECK(m.object == -1 && !m.localSpace && m.frames.empty() && m.size == 0.0625f && m.colors[0] == 0xe6e6e6ffu);
}

TEST(particles_smoke_plays_its_row_and_follows_its_parent) {
    // A SmokePoof spawned under the cave door's entrance: 3 bursts of 2; each particle plays a row
    // from its first frame to its last; moved with the entrance (local space), gone when it's off.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    const int smoke = prefab(ParticleType::SmokePoof);
    const int door = area.areaGrid.caveDoorEntrance;
    world.objects.setActive(area, door, true);
    float dx, dy;
    world.objectPosition(door, dx, dy);
    world.particles.spawnPrefab(world, ParticleType::SmokePoof, dx, dy, door);
    const float dt = 1.0f / 60;
    for (int f = 0; f < 10; f++) world.particles.update(world, dt);
    float x, y;
    CHECK(meanOf(world, smoke, x, y) == 6);
    const ParticlesDef& s = area.particles[smoke];
    for (const Particles::Particle& p : world.particles.particles())
        if (p.defIndex == smoke) {
            const int frame = Particles::frameSprite(p);
            CHECK(frame == s.frames[p.row * 5] || frame == s.frames[p.row * 5 + 1]); // (early on)
        }
    // Its parent moves 3 units right: the smoke goes along.
    world.placeObject(door, world.objects.owner(door), dx + 3, dy);
    world.particles.update(world, dt);
    float x2, y2;
    meanOf(world, smoke, x2, y2);
    check(std::fabs(x2 - x - 3) < 0.05f, "carried 3 right: " + std::to_string(x2 - x));
    // Late in their lives (from 95% on, whichever curve): the rows' last frames.
    int late = 0;
    for (int f = 0; f < 160; f++) {
        world.particles.update(world, dt);
        for (const Particles::Particle& p : world.particles.particles())
            if (p.defIndex == smoke && p.age >= p.lifetime * 0.95f) {
                late++;
                CHECK(Particles::frameSprite(p) == s.frames[p.row * 5 + 4]);
            }
    }
    CHECK(late >= 6); // (every particle passed through it)
    world.objects.setActive(area, door, false);
    world.particles.update(world, dt);
    CHECK(meanOf(world, smoke, x, y) == 0);
}

TEST(particles_village_spawns_smoke_and_sparkles) {
    // Explode: five SmokePoofs within 2 units of the entrance; a collectible appearing: one under it;
    // the cat's bubble forming: four MiniSparkles (world space: they stay where it was).
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    const int smoke = prefab(ParticleType::SmokePoof);
    village::explode(world);
    const float dt = 1.0f / 60;
    for (int f = 0; f < 10; f++) world.particles.update(world, dt);
    float x, y, ex, ey;
    world.objectPosition(area.areaGrid.caveDoorEntrance, ex, ey);
    CHECK(meanOf(world, smoke, x, y) == 30);
    CHECK(std::fabs(x - ex) < 2.5f && std::fabs(y - ey) < 2.5f);

    World w2(area, DEFAULT_SETTINGS);
    w2.setTileActive(3, true);
    const int slider = area.collectible("Slider 4");
    CHECK(slider >= 0);
    if (slider < 0) return;
    w2.objects.setActive(area, area.collectibles[slider].object, false); // (hidden until Archibald hands it out)
    village::activateCollectible(w2, "Slider 4", true);
    for (int f = 0; f < 10; f++) w2.particles.update(w2, dt);
    float sx, sy;
    w2.objectPosition(area.collectibles[slider].object, sx, sy);
    CHECK(meanOf(w2, smoke, x, y) == 6 && std::fabs(x - sx) < 1 && std::fabs(y - sy) < 1);
    village::activateCollectible(w2, "Slider 4", true); // (already out: no more smoke)
    w2.particles.update(w2, dt);
    CHECK(meanOf(w2, smoke, x, y) == 6);
}

TEST(particles_smoke_frames_blend_its_two_curves) {
    // frameOverTime's two curves (fast then slow, slow then fast), blended per particle: 30% into its
    // life one end of the blend is on frame 3 of its row (curveMin: 0.63), the other on frame 0 (0.06).
    const Area& area = villageArea();
    const int smoke = prefab(ParticleType::SmokePoof);
    if (smoke < 0) return;
    const ParticlesDef& s = area.particles[smoke];
    Particles::Particle p{};
    p.def = &s;
    p.defIndex = smoke;
    p.object = -1;
    p.age = 0.3f;
    p.lifetime = 1;
    p.color = 0xffffffffu;
    p.row = 2;
    p.frameBlend = 0;
    CHECK(Particles::frameSprite(p) == s.frames[2 * 5 + 3]);
    p.frameBlend = 1;
    CHECK(Particles::frameSprite(p) == s.frames[2 * 5 + 0]);
}

TEST(particles_prefab_bursts_truncate_and_wait_for_their_parent) {
    // MiniSparkle's bursts of 2.5 emit 2 (measured in Unity: 4 over its two, every time). A prefab
    // spawned under an inactive object waits, and plays once it turns on.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    const float dt = 1.0f / 60;
    world.particles.spawnPrefab(world, ParticleType::MiniSparkle, 0, 0, -1);
    for (int f = 0; f < 10; f++) world.particles.update(world, dt);
    float x, y;
    CHECK(meanOf(world, prefab(ParticleType::MiniSparkle), x, y) == 4);
    const int door = area.areaGrid.caveDoorEntrance;
    CHECK(!world.objects.activeInHierarchy(door));
    world.objectPosition(door, x, y);
    world.particles.spawnPrefab(world, ParticleType::SmokePoof, x, y, door);
    for (int f = 0; f < 10; f++) world.particles.update(world, dt);
    CHECK(meanOf(world, prefab(ParticleType::SmokePoof), x, y) == 0);
    world.objects.setActive(area, door, true);
    for (int f = 0; f < 10; f++) world.particles.update(world, dt);
    CHECK(meanOf(world, prefab(ParticleType::SmokePoof), x, y) == 6);
}
