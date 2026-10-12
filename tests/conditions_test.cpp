// Object active state (GameObject.SetActive with Unity's hierarchy rules), conditions, and the
// Village's start checks (ConditionChecker.checkOnStart).
#include <string>

#include "game/area.h"
#include "game/conditions.h"
#include "game/grid.h"
#include "game/object_states.h"
#include "test.h"

namespace {

int spriteIndex(const Area& area, const std::string& path) {
    for (size_t i = 0; i < area.worldSprites.size(); i++)
        if (area.worldSprites[i].path == path) return (int)i;
    return -1;
}

int colliderIndex(const Area& area, const std::string& path) {
    for (size_t i = 0; i < area.objectColliders.size(); i++)
        if (area.objectColliders[i].path == path) return (int)i;
    return -1;
}

} // namespace

TEST(object_states_start_as_authored) {
    const Area& area = village();
    ObjectStates objects;
    objects.init(area);
    int inactive = 0;
    for (size_t i = 0; i < area.worldSprites.size(); i++) {
        check(objects.spriteActive(i) == area.worldSprites[i].active, area.worldSprites[i].path);
        inactive += !area.worldSprites[i].active;
    }
    for (size_t i = 0; i < area.objectColliders.size(); i++)
        check(objects.colliderActive(i) == area.objectColliders[i].active, area.objectColliders[i].path);
    check(inactive > 0, "some world sprites start inactive (" + std::to_string(inactive) + ")");
}

TEST(object_states_follow_the_hierarchy) {
    // A/B is inactive itself; A/C is active. Hiding A hides both; showing A again shows only C.
    Area area;
    WorldSprite b, c, other;
    b.path = "A/B";
    b.active = false;
    b.inactiveAt = {"A/B"};
    c.path = "A/C";
    other.path = "AB"; // shares the prefix but is not under A
    area.worldSprites = {b, c, other};
    ObjectStates objects;
    objects.init(area);
    CHECK(!objects.spriteActive(0) && objects.spriteActive(1) && objects.spriteActive(2));
    objects.setActive(area, "A", false);
    CHECK(!objects.spriteActive(0) && !objects.spriteActive(1) && objects.spriteActive(2));
    objects.setActive(area, "A", true);
    CHECK(!objects.spriteActive(0) && objects.spriteActive(1));
    objects.setActive(area, "A/B", true);
    CHECK(objects.spriteActive(0));
    objects.setActive(area, "A", false);
    CHECK(!objects.spriteActive(0) && !objects.spriteActive(1));
}

TEST(conditions_check) {
    const Area& area = village();
    Grid grid;
    grid.init(area);
    GameState state;
    using T = ConditionDef::Type;
    ConditionDef flag{T::Flag, "caveDoorExploded", "", "", {}};
    ConditionDef noFlag{T::NoFlag, "caveDoorExploded", "", "", {}};
    CHECK(!checkCondition(flag, state, grid) && checkCondition(noFlag, state, grid));
    state.setFlag("caveDoorExploded", true);
    CHECK(checkCondition(flag, state, grid) && !checkCondition(noFlag, state, grid));
    state.setFlag("caveDoorExploded", false);
    CHECK(!checkCondition(flag, state, grid));

    ConditionDef slider7{T::Item, "Slider 7", "Village", "", {}};
    ConditionDef license{T::Item, "Boating License", "Caves", "", {}};
    CHECK(!checkCondition(slider7, state, grid));
    grid.setTileActive(7, true);
    CHECK(checkCondition(slider7, state, grid) && !checkCondition(license, state, grid));

    ConditionDef grid1{T::Grid, "", "", "...._...._..1.", {}}; // never matches a 3x3 string
    ConditionDef anyGrid{T::Grid, "", "", "", {}};
    CHECK(!checkCondition(grid1, state, grid));
    CHECK(checkCondition(anyGrid, state, grid) == CheckGrid::contains(grid.gridString(), ""));
    CHECK(!checkCondition({T::Unsupported, "", "", "", {}}, state, grid));
}

TEST(start_checks_cave_door) {
    // Rocks Blocking Door's checker: without caveDoorExploded the rocks stay and the scene change
    // door (with its dither shadow) is hidden; with it, the other way round.
    const Area& area = village();
    const std::string door = "Map/World Grid/Alcove/Cave Door/";
    const int rocks = spriteIndex(area, door + "Rocks Blocking Door");
    const int dither = spriteIndex(area, door + "Scene Change Collider/Dither");
    const int sceneChange = spriteIndex(area, door + "Scene Change Collider");
    const int rocksCol = colliderIndex(area, door + "Rocks Blocking Door");
    check(rocks >= 0 && dither >= 0 && sceneChange >= 0 && rocksCol >= 0, "cave door objects exported");
    if (rocks < 0 || dither < 0 || sceneChange < 0 || rocksCol < 0) return;
    check(area.worldSprites[dither].active, "authored: the dither is active (both shown without the check)");

    Grid grid;
    grid.init(area);
    GameState state;
    ObjectStates objects;
    objects.init(area);
    runStartChecks(area, state, grid, objects);
    check(objects.spriteActive(rocks) && objects.colliderActive(rocksCol), "new game: rocks shown and solid");
    check(!objects.spriteActive(dither) && !objects.spriteActive(sceneChange), "new game: door and dither hidden");

    state.setFlag("caveDoorExploded", true);
    objects.init(area);
    runStartChecks(area, state, grid, objects);
    check(!objects.spriteActive(rocks) && !objects.colliderActive(rocksCol), "exploded: rocks gone");
    check(objects.spriteActive(dither) && objects.spriteActive(sceneChange), "exploded: door and dither shown");
}

TEST(start_checks_rock_and_hole_follow_slider_7) {
    const Area& area = village();
    const std::string base = "Map/Village 8Puzzle/STile (5)/Objects/";
    int rock = -1, hole = -1;
    for (size_t i = 0; i < area.worldSprites.size(); i++) {
        if (area.worldSprites[i].path.rfind(base + "Rock/", 0) == 0 || area.worldSprites[i].path == base + "Rock") rock = (int)i;
        if (area.worldSprites[i].path.rfind(base + "Hole/", 0) == 0 || area.worldSprites[i].path == base + "Hole") hole = (int)i;
    }
    check(rock >= 0 && hole >= 0, "tile 5 has the rock and the hole");
    if (rock < 0 || hole < 0) return;
    Grid grid;
    grid.init(area);
    GameState state;
    ObjectStates objects;
    objects.init(area);
    runStartChecks(area, state, grid, objects);
    check(objects.spriteActive(rock) && !objects.spriteActive(hole), "without Slider 7: rock, no hole");
    grid.setTileActive(7, true);
    objects.init(area);
    runStartChecks(area, state, grid, objects);
    check(!objects.spriteActive(rock) && objects.spriteActive(hole), "with Slider 7: hole, no rock");
}
