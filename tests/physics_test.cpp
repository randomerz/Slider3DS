// Collision against the real exported Village. Every "blocked" check has a matching "free"
// control, so a world that turned fully solid (or fully open) fails.
#include <cmath>
#include <string>

#include "game/area.h"
#include "game/grid.h"
#include "game/physics.h"
#include "test.h"

namespace {

void spawnPos(const Grid& grid, const Spawn& s, float& x, float& y) {
    grid.ownerBase(s.owner, x, y);
    x += s.x;
    y += s.y;
}

// Walks from (x, y) in steps of (dx, dy) until blocked (max 600 frames); returns frames moved.
int walk(const Area& area, const Grid& grid, float& x, float& y, float dx, float dy, bool onWater) {
    int frames = 0;
    while (frames < 600 && moveWithCollision(area, grid, x, y, dx, dy, onWater)) frames++;
    return frames;
}

} // namespace

TEST(physics_spawns_start_free) {
    // Spawns on tiles that aren't collected yet (e.g. VillageWaterfall on tile 6) are only used
    // once they are, so check with every tile active.
    const Area& area = village();
    Grid grid;
    grid.init(area);
    for (const STileDef& d : area.stiles) grid.setTileActive(d.islandId, true);
    check(area.spawns.size() >= 4, "has spawns");
    for (const Spawn& s : area.spawns) {
        float x, y;
        spawnPos(grid, s, x, y);
        Blocker b = segmentBlocker(area, grid, x, y, 0, 0, s.inBoat);
        check(!b, "spawn " + s.name + " starts free (blocked by " + describe(area, b) + ")");
    }
}

TEST(physics_house_walls_stop_the_player) {
    // From the Default spawn (inside tile 1's house, beside the bed) the player can walk some way
    // right, up and down, and is always stopped before leaving the house; left, the bed (an
    // object collider) stops it after half a unit.
    const Area& area = village();
    Grid grid;
    grid.init(area);
    const Spawn* def = area.spawn("Default");
    check(def != nullptr, "Default spawn exists");
    if (!def) return;
    const float step = 5.0f / 60.0f;
    const float dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    const char* names[4] = {"right", "left", "up", "down"};
    for (int d = 0; d < 4; d++) {
        float x, y;
        spawnPos(grid, *def, x, y);
        const float sx = x, sy = y;
        int frames = walk(area, grid, x, y, dirs[d][0] * step, dirs[d][1] * step, false);
        float dist = std::fabs(x - sx) + std::fabs(y - sy);
        const std::string by = describe(area, segmentBlocker(area, grid, x, y, dirs[d][0] * step, dirs[d][1] * step, false));
        const bool left = d == 1;
        check(frames < 600 && dist > (left ? 0.3f : 1.0f) && dist < (left ? 0.7f : 17.0f) &&
                  (by.find("DoubleBed") != std::string::npos) == left,
              std::string("walk ") + names[d] + " from Default moves, then stops (" + std::to_string(dist) +
                  " units, at " + by + ")");
    }
}

TEST(physics_object_colliders_block) {
    // Every active object collider blocks a step into its middle from its own position (start
    // inside counts, like Unity's queriesStartInColliders) once its tile is collected; a collider
    // on an uncollected tile is off (the slot itself is solid instead), and so is one whose tile
    // slides in a move the player isn't riding.
    const Area& area = village();
    check(area.objectColliders.size() > 100, "village has object colliders (" + std::to_string(area.objectColliders.size()) + ")");
    Grid grid;
    grid.init(area);
    for (const STileDef& d : area.stiles) grid.setTileActive(d.islandId, true);
    int tested = 0;
    for (size_t i = 0; i < area.objectColliders.size(); i++) {
        const ObjectCollider& c = area.objectColliders[i];
        if (!c.active || c.kind == ObjectCollider::Kind::Edge) continue;
        float bx, by;
        grid.ownerBase(c.owner, bx, by);
        const float x = bx + (c.minX + c.maxX) * 0.5f, y = by + (c.minY + c.maxY) * 0.5f;
        if (c.kind == ObjectCollider::Kind::Poly && c.paths.size() == 1 && c.paths[0].size() == 8) { // boxes
            Blocker b = segmentBlocker(area, grid, x, y, 0, 0, false);
            check((bool)b, c.path + ": its center is blocked");
            tested++;
        }
    }
    check(tested > 100, "tested the box colliders (" + std::to_string(tested) + ")");

    // Tile 9's well: blocked with tile 9 collected and still; free while tile 9 is uncollected (the
    // slot is solid, reported as the inactive tile, not the well).
    const ObjectCollider* well = nullptr;
    for (const ObjectCollider& c : area.objectColliders)
        if (c.path.find("STile (9)/All Tile Maps/Tile Maps/Decorations/Well") != std::string::npos) well = &c;
    check(well != nullptr, "tile 9 has the well");
    if (!well) return;
    float bx, by;
    grid.ownerBase(9, bx, by);
    const float wx = bx + (well->minX + well->maxX) * 0.5f, wy = by + well->minY - 0.5f;
    check(!segmentBlocked(area, grid, wx, wy, 0, 0, false), "below the well is free");
    Blocker b = segmentBlocker(area, grid, wx, wy, 0, 1.0f, false);
    check(b.kind == Blocker::Object && describe(area, b).find("Well") != std::string::npos,
          "walking up into the well is blocked by it (" + describe(area, b) + ")");
    grid.setTileActive(9, false);
    b = segmentBlocker(area, grid, wx, wy, 0, 1.0f, false);
    check(b.kind == Blocker::InactiveTile, "uncollected tile 9: its slot blocks instead (" + describe(area, b) + ")");
}

TEST(physics_inactive_tile_is_solid) {
    // Walking from tile 8's slot (center 0,17, collected here) into tile 9's slot (center 17,17):
    // the slot's left edge blocks while tile 9 is uncollected, and the tile itself does not once it is.
    const Area& area = village();
    Grid grid;
    grid.init(area);
    grid.setTileActive(8, true);
    const TileState* t9 = grid.tile(9);
    check(t9 && !t9->active, "tile 9 exists and starts inactive");
    if (!t9) return;
    const float edge = t9->posX * grid.tileWidth() - grid.tileWidth() * 0.5f;
    Blocker b = segmentBlocker(area, grid, edge - 0.25f, 17.0f, 0.5f, 0.0f, false);
    check(b.kind == Blocker::InactiveTile && b.index == 9, "inactive tile 9 blocks (" + describe(area, b) + ")");
    grid.setTileActive(9, true);
    b = segmentBlocker(area, grid, edge - 0.25f, 17.0f, 0.5f, 0.0f, false);
    check(b.kind != Blocker::InactiveTile, "collected tile 9 no longer blocks as a slot (" + describe(area, b) + ")");
}

TEST(physics_raft_stops_at_the_coast) {
    // On water the land colliders apply: sailing west from the Beach spawn (on the ocean) moves,
    // then stops at the coast instead of crossing onto land. On foot the same path is open water.
    const Area& area = village();
    Grid grid;
    grid.init(area);
    const Spawn* beach = area.spawn("VillageBeach");
    check(beach != nullptr, "VillageBeach spawn exists");
    if (!beach) return;
    float x, y;
    spawnPos(grid, *beach, x, y);
    const float startX = x;
    int frames = walk(area, grid, x, y, -5.0f / 60.0f, 0, true);
    Blocker b = segmentBlocker(area, grid, x, y, -5.0f / 60.0f, 0, true);
    check(frames > 6 && frames < 600 && b.kind == Blocker::Layer,
          "raft sails west " + std::to_string(startX - x) + " units, then is blocked by " + describe(area, b));
    float fx, fy;
    spawnPos(grid, *beach, fx, fy);
    check(!segmentBlocked(area, grid, fx, fy, 0, 0, false), "the Beach spawn point itself is not solid on foot");
}

TEST(physics_slide_fences_block_in_world_and_house) {
    const Area& area = village();
    Grid grid;
    grid.init(area);
    // Everything collected except 9 (the empty slot, middle), then slide tile 7 (2,1) left into it.
    for (const STileDef& d : area.stiles) grid.setTileActive(d.islandId, true);
    grid.setTileActive(9, false);
    Blocker before = segmentBlocker(area, grid, 25.0f, 7.0f, 0.0f, 3.0f, false);
    grid.startMove(SMove::swap(2, 1, 1, 1, 7, 9), 0, 1);
    // The move covers slots (1,1) and (2,1); their bottom edge is at y = 8.5.
    Blocker world = segmentBlocker(area, grid, 25.0f, 7.0f, 0.0f, 3.0f, false);
    Blocker house = segmentBlocker(area, grid, 25.0f, 7.0f + grid.housingOffset(), 0.0f, 3.0f, false);
    check(before.kind != Blocker::Wall, "no fence before the slide (" + describe(area, before) + ")");
    check(world.kind == Blocker::Wall, "fence blocks entering the slide (" + describe(area, world) + ")");
    check(house.kind == Blocker::Wall, "fence blocks in the house copy too (" + describe(area, house) + ")");
    while (grid.moving()) grid.update(1.0f / 60.0f);
    check(grid.walls().empty(), "fences drop when the slide ends");
}
