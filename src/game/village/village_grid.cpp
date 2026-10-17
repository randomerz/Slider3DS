#include "game/village/village_grid.h"

#include "game/village/cat_trick.h"
#include "game/village/knot_box.h"
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
    world.state.setFlag(CAVE_DOOR_EXPLODED_FLAG, true);
    world.effects.shake(1, 3.5f); // (and the explosion sound, smoke)
}

void waterfallEntry(World& world) {
    if (world.state.hasCollectible("Slider 9", world.area.name)) world.state.setFlag(CAVE_DOOR_EXPLODED_FLAG, true);
}

void houseExit(World& world) { world.intro.houseExit(world); }

bool checkSpec(const World& world, const ConditionDef& c) {
    switch (c.spec) {
    case ConditionDef::Spec::VillageFishOn: return world.state.flag(FISH_ON_FLAG); // CheckFishOn
    case ConditionDef::Spec::KnotSolved: return world.knots.badLines(c.data) == 0;      // KnotBox.CheckPuzzle
    case ConditionDef::Spec::KnotPartlySolved: return world.knots.badLines(c.data) <= 3; // CheckPuzzlePartial
    case ConditionDef::Spec::RuinsRodsOn: return world.ruins.rodsOn(c.data);              // RuinsArrow.CheckActiveRods
    case ConditionDef::Spec::ChadFell: return world.chad.fell(c.data);                   // ChadJump.ChadFell
    case ConditionDef::Spec::ChadTipped: return world.chad.slidAfterJumping(c.data);     // DidSlideAfterJumping
    case ConditionDef::Spec::CatMittens: return world.state.flag(CAT_FLAGS[0]);          // CatTrick.CheckMittens
    case ConditionDef::Spec::CatAutumn: return world.state.flag(CAT_FLAGS[1]);           // CheckAutumn
    case ConditionDef::Spec::CatTofu: return world.state.flag(CAT_FLAGS[2]);             // CheckTofu
    }
    return false;
}

bool specReadsOnlyStateAndGrid(ConditionDef::Spec spec) {
    switch (spec) {
    case ConditionDef::Spec::VillageFishOn: return true; // a GameState flag
    case ConditionDef::Spec::KnotSolved:
    case ConditionDef::Spec::KnotPartlySolved: return false; // where the knot's nodes are
    case ConditionDef::Spec::RuinsRodsOn: return false;      // the rods as of the last tile arrival
    case ConditionDef::Spec::ChadFell:
    case ConditionDef::Spec::ChadTipped: return false;       // Chad's state
    case ConditionDef::Spec::CatMittens:
    case ConditionDef::Spec::CatAutumn:
    case ConditionDef::Spec::CatTofu: return true;           // GameState flags (CatTrick's save keys)
    }
    return false;
}

} // namespace village
