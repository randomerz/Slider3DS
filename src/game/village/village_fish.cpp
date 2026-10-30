#include "game/village/village_fish.h"

#include "game/village/village_grid.h"
#include "game/world/world.h"

namespace village {

namespace {

void spawnersOn(World& world) { // ParticleSpawner.SetFishOn
    for (int object : world.area.areaGrid.fishSpawners) world.objects.setActive(world.area, object, true);
}

} // namespace

void VillageFish::start(World& world) {
    if (world.state.flag(FISH_ON_FLAG)) spawnersOn(world);
}

void VillageFish::turnOn(World& world) {
    if (world.state.flag(FISH_ON_FLAG)) return;
    world.state.setFlag(FISH_ON_FLAG, true);
    spawnersOn(world);
    next_ = 0;
    wait_.start(2, world.frame);
}

void VillageFish::coroutines(World& world, float dt) {
    if (next_ < 0 || !wait_.done(world.frame, dt)) return;
    const std::vector<int>& special = world.area.areaGrid.specialFish;
    if (next_ < (int)special.size() && special[next_] >= 0) world.particles.play(world, special[next_]);
    if (++next_ < (int)special.size()) wait_.start(0.5f, world.frame);
    else next_ = -1;
}

} // namespace village
