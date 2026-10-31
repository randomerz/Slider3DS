// NPC walks (NpcWalks) on the exported Village: Chad's walk to the rocks once the Village is solved
// (no crossings, a repeated point), Romeo's StartValidWalk onto tile 1 (a crossing: he's parented to
// the tile when he steps onto it; sliding the tile first sends him to the end), one walk at a time,
// StartValidWalk waiting for the tiles to stop.
#include <cmath>
#include <string>

#include "game/world/world.h"
#include "test.h"

namespace {

constexpr float DT = 1.0f / 60;

int npcNamed(const Area& area, const std::string& name) {
    for (int i = 0; i < (int)area.npcs.size(); i++)
        if (area.npcs[i].name == name) return i;
    return -1;
}

bool near(const World& world, int a, int b) {
    float ax, ay, bx, by;
    world.objectPosition(a, ax, ay);
    world.objectPosition(b, bx, by);
    return std::fabs(ax - bx) < 1e-3f && std::fabs(ay - by) < 1e-3f;
}

bool walkingParam(const World& world, int npc) {
    return world.animators.param(world.area, world.area.npcs[npc].animator, "isWalking");
}

// Frames until `npc` stops walking (-1: still walking after 30 s).
int walkFrames(World& world, int npc) {
    for (int f = 1; f <= 1800; f++) {
        world.update(DT, 0, 0, false);
        if (!world.walks.walking(npc)) return f;
    }
    return -1;
}

// Every tile collected, laid out as `grid`.
World villageWith(const std::string& grid) {
    World world(villageArea(), DEFAULT_SETTINGS);
    for (int id = 2; id <= 9; id++) world.setTileActive(id, true);
    check(world.setGrid(grid), "set the grid to " + grid);
    return world;
}

} // namespace

TEST(walks_export) {
    const Area& area = villageArea();
    const NpcDef& chad = area.npcs.at(npcNamed(area, "Chad"));
    CHECK(chad.speed == 4 && chad.sprite >= 0 && chad.conds.at(4).walks.size() == 1);
    const NpcWalkDef& cw = chad.conds[4].walks[0];
    CHECK(cw.path.size() == 8 && cw.crossings.empty() && cw.turnAround && !cw.teleportIfInterrupted);
    CHECK(cw.path[5] == cw.path[6]); // (authored twice: a zero-length segment)
    CHECK(area.objects[cw.path.back()].path == "Map/World Grid/Objects/ChadRocks");
    const NpcDef& romeo = area.npcs.at(npcNamed(area, "Romeo"));
    CHECK(romeo.speed == 3 && romeo.conds.at(1).walks.size() == 1);
    const NpcWalkDef& rw = romeo.conds[1].walks[0];
    CHECK(rw.teleportIfInterrupted && rw.crossings.size() == 1 && rw.onFinished.size() == 1);
    const NpcCrossingDef& x = rw.crossings[0];
    CHECK(x.from == 0 && x.fromX == 2 && x.fromY == 3 && x.to == 1 && x.dirs.size() == 1 && x.dirs[0].second == -1);
    CHECK(romeo.conds[1].onEnter.size() == 1 && romeo.conds[1].onEnter[0].method == CallDef::Method::NpcStartValidWalk);
}

TEST(walks_chad_walks_to_the_rocks_when_the_village_is_solved) {
    World world = villageWith("624_897_153");
    const int chad = npcNamed(world.area, "Chad");
    const NpcDef& n = world.area.npcs[chad];
    const NpcWalkDef& w = n.conds[4].walks[0];
    CHECK(world.npcs.currentCond(chad) == 0);
    world.update(DT, 0, 0, false); // the grid changed: cond 4, whose onConditionalEnter starts the walk
    CHECK(world.npcs.currentCond(chad) == 4 && world.walks.walking(chad) && walkingParam(world, chad));
    CHECK(near(world, n.object, w.path[0])); // (the first step is at the path's start)

    // A step per frame, each segment at `speed` (a repeated point takes one step), and one more to finish.
    int expected = 1;
    for (size_t i = 1; i < w.path.size(); i++) {
        float ax, ay, bx, by;
        world.objectPosition(w.path[i - 1], ax, ay);
        world.objectPosition(w.path[i], bx, by);
        const float length = std::hypot(bx - ax, by - ay);
        expected += length > 0 ? (int)std::ceil(length / (n.speed * DT)) : 1;
    }
    const int frames = walkFrames(world, chad) + 1; // (+ the frame it started in)
    check(std::abs(frames - expected) <= 2, "walk frames " + std::to_string(frames) + " vs " + std::to_string(expected));
    // At the rocks, parented to them, turned around (turnAroundAfterWalking), no longer walking.
    CHECK(near(world, n.object, w.path.back()) && world.objects.parent(n.object) == w.path.back());
    CHECK(world.objects.owner(n.object) == 0 && !walkingParam(world, chad));
    CHECK(world.objects.flipX(n.sprite)); // the last leg faces right (his default), then he turns
}

TEST(walks_face_the_way_they_go) {
    World world = villageWith("624_897_153");
    const int chad = npcNamed(world.area, "Chad");
    const NpcDef& n = world.area.npcs[chad];
    const NpcWalkDef& w = n.conds[4].walks[0];
    // Up (ChadWalk0 -> 1): the facing is kept (as authored); then left (-> ChadWalk2): flipped (he
    // faces right by default). Reaching a point and stepping toward the next are one frame.
    world.update(DT, 0, 0, false);
    world.update(DT, 0, 0, false);
    CHECK(world.walks.walking(chad) && !world.objects.flipX(n.sprite)); // (authored unflipped)
    for (int f = 0; f < 600 && world.objects.parent(n.object) != w.path[1]; f++) world.update(DT, 0, 0, false);
    CHECK(world.objects.flipX(n.sprite));
}

TEST(walks_romeo_crosses_onto_tile_1) {
    // Tile 1 in the top-right slot (and still): Romeo's cond 1 starts the walk from above the grid.
    World world(villageArea(), DEFAULT_SETTINGS);
    const int romeo = npcNamed(world.area, "Romeo");
    const NpcDef& n = world.area.npcs[romeo];
    const NpcWalkDef& w = n.conds[1].walks[0];
    CHECK(world.npcs.currentCond(romeo) == 0 && world.objects.owner(n.object) == 0);
    CHECK(world.setGrid("641_892_753"));
    world.update(DT, 0, 0, false);
    CHECK(world.npcs.currentCond(romeo) == 1 && world.walks.walking(romeo));
    // Off the grid until he steps onto tile 1: then he's on it (parented to it, not to a path point).
    int f = 0;
    for (; f < 600 && world.objects.owner(n.object) != 1; f++) world.update(DT, 0, 0, false);
    CHECK(world.walks.walking(romeo) && world.objects.parent(n.object) == -1);
    float x, y;
    world.objectPosition(n.object, x, y);
    CHECK(world.grid.stileUnder(x, y) == 1);
    // At the end: on RomeoEnd, and its onPathFinished set the flag that moves him to cond 2.
    CHECK(walkFrames(world, romeo) > 0);
    CHECK(world.objects.parent(n.object) == w.path.back() && world.state.flag("villageRomeoAtDefaultPosition"));
    world.update(DT, 0, 0, false);
    CHECK(world.npcs.currentCond(romeo) == 2);
}

TEST(walks_sliding_a_crossing_tile_sends_romeo_to_the_end) {
    World world(villageArea(), DEFAULT_SETTINGS);
    const int romeo = npcNamed(world.area, "Romeo");
    const NpcDef& n = world.area.npcs[romeo];
    const NpcWalkDef& w = n.conds[1].walks[0];
    CHECK(world.setGrid("641_892_753"));
    for (int f = 0; f < 5; f++) world.update(DT, 0, 0, false);
    CHECK(world.walks.walking(romeo) && world.objects.owner(n.object) == 0);
    // Tile 1 slides left (its crossing is still ahead): the walk breaks, he teleports to the end
    // (on the moving tile) and the walk finishes.
    CHECK(world.artifact.tryMove(world.grid, 2, 2, 1, 2, world.player.stile));
    world.update(DT, 0, 0, false);
    CHECK(!world.walks.walking(romeo) && world.objects.parent(n.object) == w.path.back());
    CHECK(world.state.flag("villageRomeoAtDefaultPosition") && near(world, n.object, w.path.back()));
}

TEST(walks_one_at_a_time) {
    World world = villageWith("624_897_153");
    const int chad = npcNamed(world.area, "Chad");
    const NpcDef& n = world.area.npcs[chad];
    world.update(DT, 0, 0, false);
    world.update(DT, 0, 0, false);
    float x0, y0, x1, y1;
    world.objectPosition(n.object, x0, y0);
    world.walks.start(world, chad, 0); // already walking: ignored (no restart from the path's start)
    world.objectPosition(n.object, x1, y1);
    CHECK(world.walks.walking(chad) && x0 == x1 && y0 == y1);
    world.walks.start(world, chad, 5); // no such walk: ignored
    CHECK(walkFrames(world, chad) > 0);

}

TEST(walks_valid_walk_waits_for_still_tiles) {
    // StartValidWalk while tile 1 slides (Romeo in cond 0, whose one walk has no crossings): it
    // starts once no tile moves (in the frame the slide ends: the slot checks after the tiles moved).
    World world(villageArea(), DEFAULT_SETTINGS);
    const int romeo = npcNamed(world.area, "Romeo");
    const NpcDef& n = world.area.npcs[romeo];
    CHECK(world.npcs.currentCond(romeo) == 0);
    CHECK(world.artifact.tryMove(world.grid, 0, 0, 1, 0, world.player.stile));
    world.update(DT, 0, 0, false);
    CHECK(world.grid.moving());
    world.walks.startValid(world, romeo);
    CHECK(!world.walks.walking(romeo));
    for (int f = 0; f < 600 && world.grid.moving(); f++) {
        check(!world.walks.walking(romeo), "not while a tile moves");
        world.update(DT, 0, 0, false);
    }
    CHECK(world.walks.walking(romeo) && near(world, n.object, n.conds[0].walks[0].path[0]));
}

TEST(walks_broken_walks_stop_and_resume) {
    // No Village walk stops and resumes (Romeo's teleports), so on a copy whose walk doesn't: sliding
    // tile 1 away stops him; it arriving back where the crossing holds resumes him from where he
    // stood; sliding it again breaks the walk again (the port's fix: Unity's resume leaves
    // isWalking off); then he finishes.
    Area area = villageArea();
    const int romeo = npcNamed(area, "Romeo");
    area.npcs[romeo].conds[1].walks[0].teleportIfInterrupted = false;
    World world(area, DEFAULT_SETTINGS);
    const NpcDef& n = area.npcs[romeo];
    CHECK(world.setGrid("641_892_753"));
    for (int f = 0; f < 5; f++) world.update(DT, 0, 0, false);
    CHECK(world.walks.walking(romeo) && world.objects.owner(n.object) == 0);

    auto slide = [&](int fromX, int toX) {
        check(world.artifact.tryMove(world.grid, fromX, 2, toX, 2, world.player.stile), "slide tile 1");
        world.update(DT, 0, 0, false);
    };
    float x0, y0, x, y;
    slide(2, 1); // away: broken
    world.objectPosition(n.object, x0, y0);
    CHECK(!world.walks.walking(romeo) && !walkingParam(world, romeo));
    for (int f = 0; f < 600 && world.grid.moving(); f++) world.update(DT, 0, 0, false);
    world.update(DT, 0, 0, false);
    world.objectPosition(n.object, x, y);
    CHECK(!world.walks.walking(romeo) && x == x0 && y == y0); // (tile 1 in the wrong slot: no resume)
    slide(1, 2); // back
    int f = 0;
    for (; f < 600 && !world.walks.walking(romeo); f++) world.update(DT, 0, 0, false);
    world.objectPosition(n.object, x, y);
    CHECK(world.walks.walking(romeo) && walkingParam(world, romeo) && std::fabs(x - x0) < 1e-3f && std::fabs(y - y0) < 1e-3f);
    slide(2, 1); // and away again, before he's on it: broken again
    CHECK(!world.walks.walking(romeo) && world.objects.owner(n.object) == 0);
    for (f = 0; f < 600 && world.grid.moving(); f++) world.update(DT, 0, 0, false);
    slide(1, 2);
    for (f = 0; f < 600 && !world.walks.walking(romeo); f++) world.update(DT, 0, 0, false);
    CHECK(walkFrames(world, romeo) > 0);
    CHECK(world.objects.parent(n.object) == n.conds[1].walks[0].path.back() &&
          world.state.flag("villageRomeoAtDefaultPosition"));
}
