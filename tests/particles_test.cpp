// Particles (ParticleSystem playback) with the Village's systems: the knot sparkles (SparkleEffect:
// bursts, motion slowed by limit-velocity, the fade, Play while playing, stopping with the object),
// the ruins' looping sparkles (rate, force, Stop), play on awake, spawned copies and random colors;
// ParticleManager's prefabs (SmokePoof: a texture sheet, local space; MiniSparkle) and where the
// Village spawns them; the fish (a sprite sheet, an arc, orbital motion, a start delay, burst
// probability) against Unity's measurements (Slider3DSSceneSurvey.SurveyFishMotionBatch).
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
    // Every collectible's sparkles play on awake (looping, a steady rate, drifting up), and the
    // river's 44 fish (under ParticleSpawner's inactive spawners); systems the port can't show
    // (chimney smoke: emission over distance, noise; waves: their shape) are left out.
    const Area& area = villageArea();
    const int s3 = particlesOn(area, "Slider 3 Collectible/Slider Sprite/SparklesConstant Variant");
    CHECK(s3 >= 0);
    const ParticlesDef& d = area.particles[s3];
    CHECK(d.playOnAwake && d.loop && d.rateMin == 2 && d.rateMax == 3 && d.forceY == 1 && d.bursts.empty());
    CHECK(particlesOn(area, "SmokeChimney") < 0 && particlesOn(area, "FishParticles/FishParticle") >= 0);
    int onAwake = 0, fish = 0;
    for (const ParticlesDef& p : area.particles) {
        onAwake += p.playOnAwake;
        fish += p.object >= 0 && area.pathOf(p.object).find("/FishParticles/FishParticle") != std::string::npos;
    }
    CHECK(onAwake == 60 && fish == 44);
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

// Live particles of Area::particles[def] under `object` (a spawned prefab's parent; the scene
// start's SmokePoofs, Romeo's NPC.Teleport, are under other objects) and their mean position.
int meanOf(const World& world, int def, int object, float& x, float& y) {
    int n = 0;
    x = y = 0;
    for (const Particles::Particle& p : world.particles.particles())
        if (p.defIndex == def && p.object == object) n++, x += p.x, y += p.y;
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
    CHECK(s.frames.size() == 25 && s.tilesX == 5 && s.rotationMin < -6.28f && s.spinMin < 0 && s.spinMax > 0);
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
    CHECK(meanOf(world, smoke, door, x, y) == 6);
    const ParticlesDef& s = area.particles[smoke];
    for (const Particles::Particle& p : world.particles.particles())
        if (p.defIndex == smoke && p.object == door) {
            const int frame = Particles::frameSprite(p);
            CHECK(frame == s.frames[p.row * 5] || frame == s.frames[p.row * 5 + 1]); // (early on)
        }
    // Its parent moves 3 units right: the smoke goes along.
    world.placeObject(door, world.objects.owner(door), dx + 3, dy);
    world.particles.update(world, dt);
    float x2, y2;
    meanOf(world, smoke, door, x2, y2);
    check(std::fabs(x2 - x - 3) < 0.05f, "carried 3 right: " + std::to_string(x2 - x));
    // Late in their lives (from 95% on, whichever curve): the rows' last frames.
    int late = 0;
    for (int f = 0; f < 160; f++) {
        world.particles.update(world, dt);
        for (const Particles::Particle& p : world.particles.particles())
            if (p.defIndex == smoke && p.object == door && p.age >= p.lifetime * 0.95f) {
                late++;
                CHECK(Particles::frameSprite(p) == s.frames[p.row * 5 + 4]);
            }
    }
    CHECK(late >= 6); // (every particle passed through it)
    world.objects.setActive(area, door, false);
    world.particles.update(world, dt);
    CHECK(meanOf(world, smoke, door, x, y) == 0);
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
    const int door = area.areaGrid.caveDoorEntrance;
    world.objectPosition(door, ex, ey);
    CHECK(meanOf(world, smoke, door, x, y) == 30);
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
    const int object = area.collectibles[slider].object;
    CHECK(meanOf(w2, smoke, object, x, y) == 6 && std::fabs(x - sx) < 1 && std::fabs(y - sy) < 1);
    village::activateCollectible(w2, "Slider 4", true); // (already out: no more smoke)
    w2.particles.update(w2, dt);
    CHECK(meanOf(w2, smoke, object, x, y) == 6);
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
    CHECK(meanOf(world, prefab(ParticleType::MiniSparkle), -1, x, y) == 4);
    const int door = area.areaGrid.caveDoorEntrance;
    CHECK(!world.objects.activeInHierarchy(door));
    world.objectPosition(door, x, y);
    world.particles.spawnPrefab(world, ParticleType::SmokePoof, x, y, door);
    for (int f = 0; f < 10; f++) world.particles.update(world, dt);
    CHECK(meanOf(world, prefab(ParticleType::SmokePoof), door, x, y) == 0);
    world.objects.setActive(area, door, true);
    for (int f = 0; f < 10; f++) world.particles.update(world, dt);
    CHECK(meanOf(world, prefab(ParticleType::SmokePoof), door, x, y) == 6);
}

namespace {

// The special fish VillageGrid plays (TurnFishOn), and a river fish (Area::particles indices).
int specialFish() { return villageArea().areaGrid.specialFish.at(0); }
int riverFish() { return particlesOn(villageArea(), "STile (7)/All Tile Maps/Tile Maps/Decorations/FishParticles/FishParticle"); }

// Unity's step (measured: Slider3DSSceneSurvey.SurveyFishMotionBatch): the position turns `orbital`
// rad/s about the system, then the velocity moves it. Relative to the system, from (x, y).
void unityFishStep(float& x, float& y, float vx, float vy, float orbital, float dt) {
    const float c = std::cos(orbital * dt), s = std::sin(orbital * dt);
    const float rx = c * x - s * y, ry = s * x + c * y;
    x = rx + vx * dt;
    y = ry + vy * dt;
}

} // namespace

TEST(particles_fish_export) {
    // A river fish: every 1 s loop a burst of 1 at probability 0.1, after a 0-1 s start delay; leaving
    // the circle's 145-160 degrees at 2.5 units/s, circling 1 rad/s, turned 45 degrees (counter-clockwise:
    // Unity's -45) and turning 85 degrees/s; three 4 x 8 frames (minifish), local space. The special
    // fish: one play, 4 a second over 0.4 s.
    const Area& area = villageArea();
    CHECK(riverFish() >= 0 && specialFish() >= 0);
    if (riverFish() < 0 || specialFish() < 0) return;
    const ParticlesDef& f = area.particles[riverFish()];
    CHECK(f.playOnAwake && f.loop && f.duration == 1 && f.localSpace && f.delayMin == 0 && f.delayMax == 1);
    CHECK(f.bursts.size() == 1 && f.bursts[0].countMax == 1 && std::fabs(f.bursts[0].probability - 0.1f) < 1e-6f);
    CHECK(std::fabs(f.arcStart - 2.5307f) < 1e-3f && std::fabs(f.arcLength - 0.2618f) < 1e-3f && f.shapeRadius == 0.1f);
    CHECK(f.orbital == 1 && f.speedMin == 2.5f && f.speedMax == 2.5f && f.lifetimeMax == 1);
    CHECK(std::fabs(f.rotationMin - 0.7854f) < 1e-3f && std::fabs(f.spinMin - 1.4835f) < 1e-3f);
    CHECK(f.frames.size() == 3 && f.tilesX == 3 && area.sprites[f.frames[0]].w == 4 && area.sprites[f.frames[0]].h == 8);
    const ParticlesDef& s = area.particles[specialFish()];
    CHECK(!s.playOnAwake && !s.loop && s.duration == 0.4f && s.rateMax == 4 && s.bursts.empty() && s.frames == f.frames);
    CHECK(area.areaGrid.specialFish.size() == 2 && area.areaGrid.specialFish[1] >= 0);
}

TEST(particles_fish_leap_like_unity) {
    // Unity's special fish (seed 1): one fish 0.25 s after Play (the rate's first whole particle) at
    // (-0.0827, 0.0301) from the system, moving (-2.349, 0.855); half a second later at
    // (-1.3169, 0.1195), at 0.9833 s (-2.4102, -0.3679). The reference step reproduces those; the
    // port's fish follows it from wherever it starts.
    float ux = -0.0827f, uy = 0.0301f;
    for (int f = 1; f <= 59; f++) {
        unityFishStep(ux, uy, -2.349f, 0.855f, 1, 1.0f / 60);
        if (f == 30) check(std::fabs(ux + 1.3169f) < 2e-3f && std::fabs(uy - 0.1195f) < 2e-3f, "reference at 0.5 s");
    }
    check(std::fabs(ux + 2.4102f) < 2e-3f && std::fabs(uy + 0.3679f) < 2e-3f, "reference at 0.9833 s");

    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    world.setTileActive(7, true);
    const int k = specialFish();
    const ParticlesDef& d = area.particles[k];
    float ox, oy;
    world.objectPosition(d.object, ox, oy);
    world.particles.play(world, k);
    const float dt = 1.0f / 60;
    int frame = 0;
    while (countOf(world, k) == 0 && frame < 30) {
        world.particles.update(world, dt);
        frame++;
    }
    check(frame == 15 || frame == 16, "the first fish 0.25 s in: frame " + std::to_string(frame));
    auto fish = [&]() -> const Particles::Particle* {
        for (const Particles::Particle& q : world.particles.particles())
            if (q.defIndex == k) return &q;
        return nullptr;
    };
    const Particles::Particle* p = fish();
    CHECK(p != nullptr);
    if (!p) return;
    const float angle = std::atan2(p->vy, p->vx);
    CHECK(std::fabs(std::hypot(p->vx, p->vy) - 2.5f) < 1e-4f && angle >= 2.5307f - 1e-4f && angle <= 2.7925f + 1e-4f);
    // (the port's particle moved once in its emitting frame: the reference starts from there)
    float rx = p->x - ox, ry = p->y - oy;
    const float vx = p->vx, vy = p->vy;
    for (int f = 0; f < 50; f++) {
        world.particles.update(world, dt);
        unityFishStep(rx, ry, vx, vy, 1, dt);
    }
    p = fish();
    CHECK(p != nullptr);
    if (!p) return;
    check(std::fabs(p->x - ox - rx) < 1e-3f && std::fabs(p->y - oy - ry) < 1e-3f,
          "on Unity's path: " + std::to_string(p->x - ox) + ", " + std::to_string(p->y - oy));
    // Turned as Unity's mesh: 45 degrees counter-clockwise, then 85 degrees a second more.
    CHECK(std::fabs(p->rotation - (0.7854f + 1.4835f * p->age)) < 1e-3f);
}

TEST(particles_fish_frames_switch_where_unitys_do) {
    // The weighted frame-over-time curve: frame 1 from 0.2145 of its life, frame 2 from 0.825 (Unity:
    // between 0.2 and 0.2167, between 0.8167 and 0.8333).
    const Area& area = villageArea();
    if (specialFish() < 0) return;
    const ParticlesDef& d = area.particles[specialFish()];
    Particles::Particle p{};
    p.def = &d;
    p.defIndex = specialFish();
    p.object = -1;
    p.lifetime = 1;
    const float ages[] = {0.2f, 0.2167f, 0.8167f, 0.8333f};
    const int want[] = {0, 1, 1, 2};
    for (int i = 0; i < 4; i++) {
        p.age = ages[i];
        check(Particles::frameSprite(p) == d.frames[want[i]], "frame at " + std::to_string(ages[i]));
    }
}

TEST(particles_fish_frame_table_follows_the_curve) {
    // The table baked at load stays on the exact curves, between its samples too.
    for (const ParticlesDef& d : villageArea().particles) {
        if (d.frames.empty()) continue;
        Particles::Particle p{};
        p.def = &d;
        p.object = -1;
        p.lifetime = 1;
        p.frameBlend = 0.5f;
        auto exactFrame = [&](float t) { // (the blended curves, as frameSprite's formula)
            t = std::min(std::max(t, 0.0f), 1.0f);
            const float v = (evalKeys(d.frameCurveMin, t) + evalKeys(d.frameCurveMax, t)) * 0.5f * d.frameMultiplier;
            return std::min(std::max((int)(v * d.tilesX), 0), d.tilesX - 1);
        };
        for (int i = 0; i <= 1000; i++) { // (a switch lands within 0.2% of a life of the exact one: checked outside that)
            p.age = i / 1000.0f;
            const int frame = exactFrame(p.age);
            if (exactFrame(p.age - 0.002f) == frame && exactFrame(p.age + 0.002f) == frame)
                check(Particles::frameSprite(p) == d.frames[frame], "frame at " + std::to_string(p.age));
        }
    }
}

TEST(particles_orbital_turns_about_each_systems_own_position) {
    // Not the Village's fish (all local space, on their object): world space, under an object, at an
    // offset from one, and under none. Each particle follows the reference step about its own system,
    // also across a change of the time step.
    Area area = villageArea();
    const int river = riverFish(), special = specialFish(), smoke = area.particlePrefabs[(int)ParticleType::SmokePoof];
    CHECK(river >= 0 && special >= 0 && smoke >= 0);
    if (river < 0 || special < 0 || smoke < 0) return;
    for (ParticlesDef& d : area.particles) d.playOnAwake = false; // (only these four particles)
    for (int k : {river, special, smoke}) {
        ParticlesDef& d = area.particles[k];
        d.localSpace = d.loop = d.limitVelocity = false;
        d.orbital = 1;
        d.speedMin = d.speedMax = 1;
        d.shapeRadius = 0;
        d.lifetimeMin = d.lifetimeMax = d.duration = 10;
        d.delayMin = d.delayMax = 0;
        d.rateMin = d.rateMax = 0;
        d.forceX = d.forceY = 0;
        d.bursts = {{0, 1, 1, 1, 0}};
    }
    World world(area, DEFAULT_SETTINGS);
    world.setTileActive(7, true);
    world.objects.setActive(area, area.areaGrid.fishSpawners.at(1), true);
    world.particles.reset(); // (Romeo's start poofs)
    const int object = area.particles[river].object;
    float ax, ay, bx, by;
    world.objectPosition(object, ax, ay);
    world.objectPosition(area.particles[special].object, bx, by);
    CHECK(ax != bx || ay != by);
    world.particles.play(world, river);
    world.particles.spawnPrefab(world, ParticleType::SmokePoof, 3, 4, -1);
    world.particles.spawnPrefab(world, ParticleType::SmokePoof, ax + 2, ay + 1, object);
    world.particles.play(world, special);
    const float centers[4][2] = {{ax, ay}, {3, 4}, {ax + 2, ay + 1}, {bx, by}}; // (in emitters' order)
    float dt = 1.0f / 60;
    world.particles.update(world, dt);
    const std::vector<Particles::Particle>& live = world.particles.particles();
    CHECK(live.size() == 4);
    if (live.size() != 4) return;
    float rel[4][2];
    for (int i = 0; i < 4; i++) {
        rel[i][0] = live[i].x - centers[i][0];
        rel[i][1] = live[i].y - centers[i][1];
    }
    for (int f = 0; f < 60; f++) {
        if (f == 30) dt = 1.0f / 30;
        world.particles.update(world, dt);
        for (int i = 0; i < 4; i++) unityFishStep(rel[i][0], rel[i][1], live[i].vx, live[i].vy, 1, dt);
    }
    for (int i = 0; i < 4; i++)
        check(std::fabs(live[i].x - centers[i][0] - rel[i][0]) < 1e-3f && std::fabs(live[i].y - centers[i][1] - rel[i][1]) < 1e-3f,
              "particle " + std::to_string(i) + " on its path");
}

TEST(particles_fish_start_delays_spread_the_river) {
    // Each river fish draws its own 0-1 s delay, so they don't leap together: with every burst forced
    // to fire, the first leaps spread over the second.
    Area area = villageArea();
    int fish = 0;
    for (ParticlesDef& d : area.particles)
        if (d.playOnAwake && area.pathOf(d.object).find("/FishParticles/") != std::string::npos) {
            d.bursts.at(0).probability = 1;
            fish++;
        }
    CHECK(fish == 44);
    World world(area, DEFAULT_SETTINGS);
    for (int id : {2, 4, 6, 7}) world.setTileActive(id, true);
    for (int o : area.areaGrid.fishSpawners) world.objects.setActive(area, o, true);
    int bornByThird[4] = {}; // (how many have leapt by 1/3, 2/3, 3/3 of a second, and a little past it)
    std::vector<uint8_t> born(area.particles.size(), 0);
    const float dt = 1.0f / 60;
    for (int f = 1; f <= 62; f++) {
        world.particles.update(world, dt);
        for (const Particles::Particle& p : world.particles.particles())
            if (area.particles[p.defIndex].playOnAwake && area.pathOf(p.def->object).find("/FishParticles/") != std::string::npos && !born[p.defIndex]) {
                born[p.defIndex] = 1;
                for (int k = 0; k < 4; k++) bornByThird[k] += f <= (k < 3 ? 20 * (k + 1) : 62);
            }
    }
    check(bornByThird[3] == fish, "all leapt within the delay: " + std::to_string(bornByThird[3]));
    check(bornByThird[0] >= 7 && bornByThird[0] <= 23, "a third by 1/3 s: " + std::to_string(bornByThird[0]));
    check(bornByThird[1] >= 21 && bornByThird[1] <= 37, "two thirds by 2/3 s: " + std::to_string(bornByThird[1]));
}

TEST(particles_fish_wait_out_their_delay_and_leap_at_random) {
    // A river fish once its spawner is on: each loop's burst fires one time in ten (Unity's seeds:
    // 1-4 fish in 30 s), always the same time into the loop (its start delay, 0-1 s: drawn once).
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    world.setTileActive(7, true);
    const int k = riverFish();
    if (k < 0) return;
    world.objects.setActive(area, area.areaGrid.fishSpawners.at(1), true); // (STile 7's)
    CHECK(world.objects.activeInHierarchy(area.particles[k].object));
    const float dt = 1.0f / 60;
    std::vector<float> born;
    int most = 0;
    for (int f = 1; f <= 60 * 120; f++) {
        world.particles.update(world, dt); // (play on awake: it starts in the first)
        int n = 0;
        for (const Particles::Particle& p : world.particles.particles())
            if (p.defIndex == k) {
                n++;
                if (p.age <= dt * 1.5f) born.push_back(f * dt - p.age);
            }
        most = std::max(most, n);
    }
    check(born.size() >= 4 && born.size() <= 24, "fish in 120 s: " + std::to_string(born.size()));
    CHECK(most == 1); // (a second-long leap a loop at most)
    if (born.empty()) return;
    const float first = born[0] - std::floor(born[0]);
    for (float t : born) {
        const float into = t - std::floor(t), off = std::fabs(into - first);
        check(off < 0.02f || off > 0.98f, "the same time into each loop: " + std::to_string(into) + " vs " + std::to_string(first));
    }
}
