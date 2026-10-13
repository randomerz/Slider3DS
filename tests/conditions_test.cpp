// Object active state (GameObject.SetActive with Unity's hierarchy rules), conditions, and Starts
// (ConditionChecker.checkOnStart, Collectible.shouldDisableAtStart) on a synthetic area and the Village.
#include <string>

#include "game/area.h"
#include "game/conditions.h"
#include "game/grid.h"
#include "game/object_states.h"
#include "game/world.h"
#include "test.h"

namespace {

int spriteOf(const Area& area, const std::string& path) {
    const int object = area.findObject(path);
    for (size_t i = 0; i < area.worldSprites.size(); i++)
        if (area.worldSprites[i].object == object) return (int)i;
    return -1;
}

int colliderOf(const Area& area, const std::string& path) {
    const int object = area.findObject(path);
    for (size_t i = 0; i < area.objectColliders.size(); i++)
        if (area.objectColliders[i].object == object) return (int)i;
    return -1;
}

// Objects: 0 "A", 1 "A/B" (inactive itself), 2 "A/C", 3 "D" (inactive).
Area hierarchy() {
    Area area;
    area.objects = {{"A", -1, true}, {"A/B", 0, false}, {"A/C", 0, true}, {"D", -1, false}};
    return area;
}

} // namespace

TEST(object_states_follow_the_hierarchy) {
    // A/B is inactive itself; A/C is active. Hiding A hides both; showing A again shows only C.
    const Area area = hierarchy();
    ObjectStates o;
    o.init(area);
    CHECK(o.activeInHierarchy(0) && !o.activeInHierarchy(1) && o.activeInHierarchy(2) && !o.activeInHierarchy(3));
    o.setActive(area, 0, false);
    CHECK(!o.activeInHierarchy(0) && !o.activeInHierarchy(1) && !o.activeInHierarchy(2));
    o.setActive(area, 0, true);
    CHECK(!o.activeInHierarchy(1) && o.activeInHierarchy(2));
    o.setActive(area, 1, true);
    CHECK(o.activeInHierarchy(1));
    o.setActive(area, 0, false);
    CHECK(!o.activeInHierarchy(1) && !o.activeInHierarchy(2));

    ObjectCollider c;
    c.object = 2;
    c.enabled = false;
    o.setActive(area, 0, true);
    CHECK(o.activeInHierarchy(2) && !o.active(c)); // a disabled component stays off
}

TEST(starts_run_when_their_object_first_activates) {
    // Slider 7's collectible sits under the inactive "Burried Slider": its Start (which hides it,
    // shouldDisableAtStart) runs only when digging shows its parent, and only once.
    const Area& area = village();
    World world(area);
    const int buried = area.findObject("Map/Village 8Puzzle/STile (5)/Objects/Burried Slider");
    size_t slider7 = area.collectibles.size();
    for (size_t i = 0; i < area.collectibles.size(); i++)
        if (area.collectibles[i].name == "Slider 7") slider7 = i;
    check(buried >= 0 && slider7 < area.collectibles.size(), "the buried slider and its collectible exist");
    if (buried < 0 || slider7 == area.collectibles.size()) return;
    const int object = area.collectibles[slider7].object;
    world.setTileActive(5, true); // its tile (uncollected tiles' objects are all off)
    check(!world.starts.started(area, ComponentRef::Kind::Collectible, (int)slider7), "not started while its parent is hidden");
    world.objects.setActive(area, buried, true);
    check(world.objects.activeInHierarchy(object), "shown with its parent ...");
    world.starts.update(world);
    check(world.starts.started(area, ComponentRef::Kind::Collectible, (int)slider7) && !world.objects.activeInHierarchy(object), "... then its Start hides it");
    world.objects.setActive(area, object, true); // shown by an event later: Start doesn't run again
    world.starts.update(world);
    check(world.objects.activeInHierarchy(object), "and an event can show it afterwards");
}

TEST(conditions_check) {
    const Area& area = village();
    Grid grid;
    grid.init(area);
    GameState state;
    using T = ConditionDef::Type;
    const ConditionDef flag{T::Flag, "caveDoorExploded", "", "", {}};
    const ConditionDef noFlag{T::NoFlag, "caveDoorExploded", "", "", {}};
    CHECK(!checkCondition(flag, state, grid) && checkCondition(noFlag, state, grid));
    state.setFlag("caveDoorExploded", true);
    CHECK(checkCondition(flag, state, grid) && !checkCondition(noFlag, state, grid));
    state.setFlag("caveDoorExploded", false);
    CHECK(!checkCondition(flag, state, grid));

    const ConditionDef slider7{T::Item, "Slider 7", "Village", "", {}};
    CHECK(!checkCondition(slider7, state, grid));
    state.setCollectible("Slider 7", "Caves", true); // another area's doesn't count
    CHECK(!checkCondition(slider7, state, grid));
    state.setCollectible("Slider 7", "Village", true);
    CHECK(checkCondition(slider7, state, grid));

    // The start grid is "###_###_1##" (only tile 1, bottom-left).
    const ConditionDef bottomLeft1{T::Grid, "", "", "..._..._1..", {}};
    const ConditionDef topLeft1{T::Grid, "", "", "1.._..._...", {}};
    CHECK(checkCondition(bottomLeft1, state, grid) && !checkCondition(topLeft1, state, grid));
    const ConditionDef notTopLeft1{T::NoGrid, "", "", "1.._..._...", {}};
    CHECK(checkCondition(notTopLeft1, state, grid));
    CHECK(!checkCondition({T::Unsupported, "", "", "", {}}, state, grid));
    CHECK(!checkCondition({T::PlayerCarryingItem, "anything", "", "", {}}, state, grid));
}

TEST(conditions_grid_stationary) {
    // GridStationary: the pattern matches and the listed tiles aren't sliding. The grid string
    // changes as soon as a move starts; the animation follows.
    World world(village());
    CHECK(world.artifact.tryMove(world.grid, 0, 0, 0, 2, 0)); // tile 1 up to the top-left slot
    world.artifact.update(world.grid, 1.0f / 60.0f, 0);
    using T = ConditionDef::Type;
    const ConditionDef tile1Still{T::GridStationary, "", "", world.grid.gridString(), {1}};
    const ConditionDef tile2Still{T::GridStationary, "", "", world.grid.gridString(), {2}};
    check(world.grid.tileMoving(1) && !world.grid.tileMoving(2), "tile 1 is sliding, tile 2 isn't");
    CHECK(!checkCondition(tile1Still, world.state, world.grid) && checkCondition(tile2Still, world.state, world.grid));
    for (int i = 0; i < 600 && !world.artifact.queueEmpty(); i++) world.artifact.update(world.grid, 1.0f / 60.0f, 0);
    CHECK(!world.grid.tileMoving(1) && checkCondition(tile1Still, world.state, world.grid));
}

TEST(start_checks_cave_door) {
    // Rocks Blocking Door's checker: without caveDoorExploded the rocks stay and the scene change
    // door (with its dither shadow) is hidden; with it, the other way round.
    const Area& area = village();
    const std::string door = "Map/World Grid/Alcove/Cave Door/";
    const int rocks = spriteOf(area, door + "Rocks Blocking Door");
    const int dither = spriteOf(area, door + "Scene Change Collider/Dither");
    const int rocksCol = colliderOf(area, door + "Rocks Blocking Door");
    check(rocks >= 0 && dither >= 0 && rocksCol >= 0, "cave door objects exported");
    if (rocks < 0 || dither < 0 || rocksCol < 0) return;
    const int ditherObject = area.worldSprites[dither].object;
    check(area.objects[ditherObject].activeSelf && area.objects[area.objects[ditherObject].parent].activeSelf,
          "authored: the dither and its door are active (both shown without the check)");

    World world(area);
    check(world.objects.active(area.worldSprites[rocks]) && world.objects.active(area.objectColliders[rocksCol]),
          "new game: rocks shown and solid");
    check(!world.objects.active(area.worldSprites[dither]), "new game: door and dither hidden");

    world.state.setFlag("caveDoorExploded", true);
    world.reloadObjects();
    check(!world.objects.active(area.worldSprites[rocks]) && !world.objects.active(area.objectColliders[rocksCol]),
          "exploded: rocks gone");
    check(world.objects.active(area.worldSprites[dither]), "exploded: door and dither shown");
}

TEST(start_checks_rock_and_hole_follow_slider_7) {
    const Area& area = village();
    const int rockObject = area.findObject("Map/Village 8Puzzle/STile (5)/Objects/Rock");
    const int holeObject = area.findObject("Map/Village 8Puzzle/STile (5)/Objects/Hole");
    check(rockObject >= 0 && holeObject >= 0, "tile 5 has the rock and the hole");
    if (rockObject < 0 || holeObject < 0) return;
    World world(area);
    check(!world.objects.activeInHierarchy(rockObject) && !world.objects.activeInHierarchy(holeObject),
          "tile 5 uncollected: neither (its objects are off, STile.Awake)");
    world.setTileActive(5, true);
    check(world.objects.activeInHierarchy(rockObject) && !world.objects.activeInHierarchy(holeObject),
          "tile 5 collected, without Slider 7: rock, no hole (their checks ran on collection)");
    world.state.setCollectible("Slider 7", "Village", true);
    world.reloadObjects();
    check(!world.objects.activeInHierarchy(rockObject) && world.objects.activeInHierarchy(holeObject),
          "with Slider 7: hole, no rock");
}

TEST(start_checks_skip_the_inactive_raft) {
    // The Raft's own checker sits on an inactive object, so on a new game it never starts.
    const Area& area = village();
    World world(area);
    const int raft = area.findObject("Map/World Grid/Beach/Raft");
    check(raft >= 0, "the raft object exists");
    int found = 0;
    for (size_t i = 0; i < area.conditionCheckers.size(); i++)
        if (area.conditionCheckers[i].object == raft) {
            check(!world.starts.started(area, ComponentRef::Kind::ConditionChecker, (int)i), "the raft's check did not run");
            found++;
        }
    check(found == 1, "the raft has a checker");
}
