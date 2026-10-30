// The Village's fish (VillageGrid.TurnFishOn + ParticleSpawner.SetFishOn + SpawnSpecialFish): once the
// salmon leaves for the waterfall, ParticleSpawner's fish spawners turn on (their FishParticles play on
// awake: fish leaping along the river), and 2 s / 2.5 s later the two special fish leap by the beach.
// The save keeps only VillageGrid.fishOn (village::FISH_ON_FLAG); VillageGrid.Start turns the spawners
// back on.
#pragma once

#include "game/world/wait.h"

class World;

namespace village {

class VillageFish {
public:
    void reset() { *this = VillageFish(); }
    void start(World& world);  // VillageGrid.Start: the spawners on, once fishOn
    void turnOn(World& world); // VillageGrid.TurnFishOn (the first time)
    // SpawnSpecialFish's waits (coroutine slot).
    void coroutines(World& world, float dt);

private:
    int next_ = -1; // the special fish to play after wait_ (Area::areaGrid.specialFish index), -1 = none
    Wait wait_;
};

} // namespace village
