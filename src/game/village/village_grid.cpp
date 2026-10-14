#include "game/village/village_grid.h"

#include "game/world/world.h"

namespace village {

void activateCollectible(World& world, const std::string& name, bool clearDisableAtStart) {
    const Area& area = world.area;
    const int index = area.collectible(name);
    if (index < 0 || world.state.hasCollectible(name, area.name)) return;
    if (clearDisableAtStart) world.starts.markStarted(area, ComponentRef::Kind::Collectible, index);
    world.objects.setActive(area, area.collectibles[index].object, true);
}

void turnFishOn(World& world) {
    world.state.setFlag(FISH_ON_FLAG, true); // (and fish particles)
}

void explode(World& world) {
    const Area& area = world.area;
    world.objects.setActive(area, area.areaGrid.caveDoorEntrance, true);
    world.objects.setActive(area, area.areaGrid.caveDoorRocks, false);
    world.state.setFlag(CAVE_DOOR_EXPLODED_FLAG, true); // (and a camera shake, smoke)
}

void waterfallEntry(World& world) {
    if (world.state.hasCollectible("Slider 9", world.area.name)) world.state.setFlag(CAVE_DOOR_EXPLODED_FLAG, true);
}

void houseExit(World& world) {
    world.state.setFlag(INTRO_CUTSCENE_FLAG, true); // (the camera-dolly cutscene isn't ported yet)
}

bool checkSpec(const World& world, ConditionDef::Spec spec) {
    switch (spec) {
    case ConditionDef::Spec::VillageFishOn: return world.state.flag(FISH_ON_FLAG); // CheckFishOn
    }
    return false;
}

bool specReadsOnlyStateAndGrid(ConditionDef::Spec spec) {
    switch (spec) {
    case ConditionDef::Spec::VillageFishOn: return true; // a GameState flag
    }
    return false;
}

} // namespace village
