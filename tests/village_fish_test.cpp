// The Village's fish (village::VillageFish) on the exported Village: TurnFishOn turns ParticleSpawner's
// spawners on (their fish play on awake) and plays the two special fish 2 s and 2.5 s later; Start
// turns the spawners back on once fishOn; the salmon's "gotta blast!" leaves in two SmokePoofs
// (NPC.Teleport) and plays its river's fish (ParticleSystem.Play).
#include <string>

#include "game/events/events.h"
#include "game/village/village_grid.h"
#include "game/world/world.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60;

void frames(World& world, int n) {
    for (int i = 0; i < n; i++) world.update(DT, 0, 0, false);
}

// A world with the river's tiles on (the spawners sit on 2, 4, 6 and 7).
World riverWorld() {
    World world(villageArea(), DEFAULT_SETTINGS);
    for (int id : {2, 4, 6, 7}) world.setTileActive(id, true);
    return world;
}

bool spawnersOn(const World& world) {
    for (int o : world.area.areaGrid.fishSpawners)
        if (!world.objects.activeSelf(o)) return false;
    return !world.area.areaGrid.fishSpawners.empty();
}

// Live particles under `object`.
int particlesUnder(const World& world, int object) {
    int n = 0;
    for (const Particles::Particle& p : world.particles.particles()) n += p.object == object;
    return n;
}

} // namespace

TEST(village_fish_export) {
    // ParticleSpawner's four spawners (STiles 4, 7, 6, 2), off until TurnFishOn; the two special fish.
    const Area& area = villageArea();
    CHECK(area.areaGrid.fishSpawners.size() == 4 && area.areaGrid.specialFish.size() == 2);
    for (int o : area.areaGrid.fishSpawners) CHECK(area.pathOf(o).find("/FishParticles") != std::string::npos && !area.objects[o].activeSelf);
    for (int p : area.areaGrid.specialFish) CHECK(p >= 0 && area.pathOf(area.particles[p].object).find("Special FishParticle") != std::string::npos);
}

TEST(village_fish_turn_on_spawners_then_two_special_fish) {
    World world = riverWorld();
    const Area& area = world.area;
    const int s1 = area.areaGrid.specialFish[0], s2 = area.areaGrid.specialFish[1];
    CHECK(!spawnersOn(world));
    village::turnFishOn(world);
    CHECK(world.state.flag(village::FISH_ON_FLAG) && spawnersOn(world));
    frames(world, 1);
    int riverPlaying = 0; // (every river fish plays on awake as its spawner turns on)
    for (int i = 0; i < (int)area.particles.size(); i++)
        riverPlaying += area.particles[i].playOnAwake && area.pathOf(area.particles[i].object).find("/FishParticles/") != std::string::npos &&
                        world.particles.playing(i);
    CHECK(riverPlaying == 44);
    // SpawnSpecialFish: WaitForSeconds(2) from the next frame, then the first; 0.5 s more, the second
    // (checked a couple of frames off the boundaries: the waits sum 1/60 s steps).
    frames(world, 117);
    CHECK(!world.particles.playing(s1));
    frames(world, 5);
    CHECK(world.particles.playing(s1) && !world.particles.playing(s2));
    frames(world, 25);
    CHECK(!world.particles.playing(s2));
    frames(world, 5);
    CHECK(world.particles.playing(s2));
    // Once on, again does nothing: no second run of the special fish.
    frames(world, 120); // (both done)
    CHECK(!world.particles.playing(s1) && !world.particles.playing(s2));
    village::turnFishOn(world);
    frames(world, 200);
    CHECK(!world.particles.playing(s1));
}

TEST(village_fish_back_on_after_a_load) {
    // VillageGrid.Start: fishOn (saved) turns the spawners on; the special fish don't play again.
    World world = riverWorld();
    world.state.setFlag(village::FISH_ON_FLAG, true);
    world.reloadObjects();
    CHECK(spawnersOn(world));
    frames(world, 200);
    for (int p : world.area.areaGrid.specialFish) CHECK(!world.particles.playing(p));

    World off = riverWorld();
    off.reloadObjects();
    CHECK(!spawnersOn(off));
}

TEST(village_salmon_blasts_off_in_a_poof) {
    // Its "gotta blast!" chain: the line turns the fish on, the next teleports it away (a SmokePoof
    // where it was and where it lands, both under its destination) and plays two river fish.
    World world = riverWorld();
    const Area& area = world.area;
    const NpcDef* salmon = nullptr;
    const DialogueDef* blast = nullptr;
    const DialogueDef* leave = nullptr;
    for (const NpcDef& n : area.npcs)
        for (const NpcCondDef& c : n.conds)
            for (size_t i = 0; i + 1 < c.dialogue.size(); i++)
                if (c.dialogue[i].text.find("gotta blast") != std::string::npos) {
                    salmon = &n;
                    blast = &c.dialogue[i];
                    leave = &c.dialogue[i + 1];
                }
    CHECK(salmon && blast && leave);
    if (!salmon || !blast || !leave) return;
    runCalls(world, blast->onStart);
    CHECK(world.state.flag(village::FISH_ON_FLAG));
    int target = -1, plays = 0;
    for (const CallDef& c : leave->onStart) {
        if (c.method == CallDef::Method::NpcTeleport) target = c.objectArg;
        plays += c.method == CallDef::Method::ParticlePlay;
    }
    CHECK(target >= 0 && plays == 2);
    if (target < 0) return;
    frames(world, 30); // (Romeo's start poofs are under other objects)
    float sx, sy;
    world.objectPosition(salmon->object, sx, sy);
    runCalls(world, leave->onStart);
    float tx, ty, nx, ny;
    world.objectPosition(target, tx, ty);
    world.objectPosition(salmon->object, nx, ny);
    CHECK(world.objects.parent(salmon->object) == target && nx == tx && ny == ty);
    frames(world, 10);
    check(particlesUnder(world, target) == 12, "two poofs: " + std::to_string(particlesUnder(world, target)));
    int there = 0, here = 0; // (one at each end)
    for (const Particles::Particle& p : world.particles.particles())
        if (p.object == target) {
            there += std::hypot(p.x - sx, p.y - sy) < 1.5f;
            here += std::hypot(p.x - tx, p.y - ty) < 1.5f;
        }
    CHECK(there == 6 && here == 6);
}

TEST(village_particle_play_call_starts_its_system) {
    // ParticleSystem.Play from a UnityEvent: (in the game the line before has started them already, so
    // these stop it first) it plays the call's system, and nothing while its object is off.
    World world = riverWorld();
    std::vector<CallDef> calls;
    for (const NpcDef& n : world.area.npcs)
        for (const NpcCondDef& c : n.conds)
            for (const DialogueDef& d : c.dialogue)
                for (const CallDef& call : d.onStart)
                    if (call.method == CallDef::Method::ParticlePlay) calls.push_back(call);
    CHECK(calls.size() == 2);
    runCalls(world, calls);
    for (const CallDef& c : calls) CHECK(!world.particles.playing(c.data)); // (the spawners are off)
    village::turnFishOn(world);
    frames(world, 1);
    for (const CallDef& c : calls) {
        world.particles.stop(c.data);
        CHECK(!world.particles.playing(c.data));
    }
    runCalls(world, calls);
    for (const CallDef& c : calls) CHECK(world.particles.playing(c.data));
}
