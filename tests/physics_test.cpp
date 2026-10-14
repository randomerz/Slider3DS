// Collision against the real exported Village. Every "blocked" check has a matching "free"
// control, so a world that turned fully solid (or fully open) fails.
#include <cmath>
#include <string>

#include "game/area.h"
#include "game/grid.h"
#include "game/physics.h"
#include "game/world.h"
#include "test.h"

namespace {

constexpr float STEP = 5.0f / 60.0f; // Player.moveSpeed per frame

void spawnPos(const Grid& grid, const Spawn& s, float& x, float& y) {
    grid.ownerBase(s.owner, x, y);
    x += s.x;
    y += s.y;
}

// A thin probe box along an axis-aligned segment (or around a point).
Box probe(float x, float y, float dx = 0, float dy = 0) {
    const float r = 0.01f;
    return {std::fmin(x, x + dx) - r, std::fmin(y, y + dy) - r, std::fmax(x, x + dx) + r, std::fmax(y, y + dy) + r};
}

// Walks the foot (or boat) box from (x, y) in steps of (dx, dy) until it stops (max 600 frames);
// returns frames moved.
int walk(const World& world, float& x, float& y, float dx, float dy, bool onWater) {
    const Hitbox& hb = onWater ? world.area.player.boatBox : world.area.player.footBox;
    int frames = 0;
    float px = x, py = y;
    while (frames < 600 && moveWithCollision(world, hb, x, y, dx, dy, onWater) &&
           (std::fabs(x - px) > 1e-5f || std::fabs(y - py) > 1e-5f)) {
        px = x;
        py = y;
        frames++;
    }
    return frames;
}

// What the hitbox at (x, y) runs into one step further along (dx, dy).
Blocker ahead(const World& world, float x, float y, float dx, float dy, bool onWater) {
    const Hitbox& hb = onWater ? world.area.player.boatBox : world.area.player.footBox;
    return boxBlocker(world, hitboxAt(hb, x + dx, y + dy), onWater);
}

const ObjectCollider* findCollider(const Area& area, const std::string& pathPart) {
    for (const ObjectCollider& c : area.objectColliders)
        if (area.pathOf(c.object).find(pathPart) != std::string::npos) return &c;
    return nullptr;
}

} // namespace

TEST(physics_player_hitboxes) {
    const Area& area = village();
    const Hitbox& f = area.player.footBox;
    const Hitbox& b = area.player.boatBox;
    check(f.w == 0.75f && f.h == 0.75f && f.offY == -0.0625f, "foot box: 0.5x0.5 + 0.125 edge radius at (0,-1/16)");
    check(b.w == 1.5f && b.h == 0.75f && b.offY == -0.375f, "boat box: 1.5x0.75 at (0,-0.375)");
}

TEST(physics_spawns_start_free) {
    // Spawns on tiles that aren't collected yet (e.g. VillageWaterfall on tile 6) are only used
    // once they are, so check with every tile active.
    const Area& area = village();
    World world(area);
    Grid& grid = world.grid;
    for (const STileDef& d : area.stiles) world.setTileActive(d.islandId, true);
    check(area.spawns.size() >= 4, "has spawns");
    for (const Spawn& s : area.spawns) {
        float x, y;
        spawnPos(grid, s, x, y);
        Blocker b = boxBlocker(world, hitboxAt(s.inBoat ? area.player.boatBox : area.player.footBox, x, y), s.inBoat);
        check(!b, "spawn " + s.name + " starts free (blocked by " + describe(area, b) + ")");
    }
}

TEST(physics_house_walls_stop_the_player) {
    // From the Default spawn (inside tile 1's house, beside the bed) the player can walk some way
    // right, up (to the computer desk) and down, and is stopped flush before leaving the house;
    // left, the bed (an object collider) is just an eighth of a unit away.
    const Area& area = village();
    World world(area);
    Grid& grid = world.grid;
    const Spawn* def = area.spawn("Default");
    check(def != nullptr, "Default spawn exists");
    if (!def) return;
    const float dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    const char* names[4] = {"right", "left", "up", "down"};
    for (int d = 0; d < 4; d++) {
        float x, y;
        spawnPos(grid, *def, x, y);
        const float sx = x, sy = y;
        int frames = walk(world, x, y, dirs[d][0] * STEP, dirs[d][1] * STEP, false);
        float dist = std::fabs(x - sx) + std::fabs(y - sy);
        const Blocker touching = ahead(world, x, y, dirs[d][0] * 0.01f, dirs[d][1] * 0.01f, false);
        const std::string by = describe(area, touching);
        const bool left = d == 1;
        check(frames < 600 && dist > (left ? 0.05f : 0.5f) && dist < (left ? 0.3f : 17.0f) && touching &&
                  (by.find("DoubleBed") != std::string::npos) == left,
              std::string("walk ") + names[d] + " from Default moves, then stops flush (" + std::to_string(dist) +
                  " units, touching " + by + ")");
    }
}

TEST(physics_slides_along_walls) {
    // Walking diagonally into the house's top wall keeps the sideways part of the move (Box2D
    // pushes the box out along the wall's normal), and the box rests flush against the wall.
    const Area& area = village();
    World world(area);
    Grid& grid = world.grid;
    const Spawn* def = area.spawn("Default");
    if (!def) return;
    float x, y;
    spawnPos(grid, *def, x, y);
    walk(world, x, y, 0, STEP, false);
    check(!ahead(world, x, y, 0, 0, false) && ahead(world, x, y, 0, 0.01f, false),
          "resting flush under the wall: free here, blocked 0.01 further up");
    const float sx = x, sy = y;
    const float d = STEP / std::sqrt(2.0f);
    moveWithCollision(world, area.player.footBox, x, y, d, d, false);
    check(std::fabs(x - sx - d) < 1e-4f && std::fabs(y - sy) < 1e-3f,
          "diagonal into the wall slides right by the x part (" + std::to_string(x - sx) + ", " + std::to_string(y - sy) + ")");
}

TEST(physics_object_colliders_block) {
    // Every active box-shaped object collider blocks a probe at its center (where its object is
    // now: NPCs move at Start) once its tile is collected; a collider on an uncollected tile is off
    // (the slot itself is solid instead).
    const Area& area = village();
    check(area.objectColliders.size() > 150, "village has object colliders (" + std::to_string(area.objectColliders.size()) + ")");
    World world(area);
    Grid& grid = world.grid;
    for (const STileDef& d : area.stiles) world.setTileActive(d.islandId, true);
    int tested = 0;
    for (const ObjectCollider& c : area.objectColliders) {
        if (!world.objects.active(c) || c.kind != ObjectCollider::Kind::Poly || c.paths.size() != 1 || c.paths[0].size() != 8) continue;
        float bx, by;
        world.objectBase(c.object, bx, by);
        Blocker b = boxBlocker(world, probe(bx + (c.minX + c.maxX) * 0.5f, by + (c.minY + c.maxY) * 0.5f), false);
        check((bool)b, area.pathOf(c.object) + ": its center is blocked");
        tested++;
    }
    check(tested > 100, "tested the box colliders (" + std::to_string(tested) + ")");

    // Tile 9's well: walking up into it stops at it; with tile 9 uncollected the slot blocks instead.
    const ObjectCollider* well = findCollider(area, "STile (9)/All Tile Maps/Tile Maps/Decorations/Well");
    check(well != nullptr, "tile 9 has the well");
    if (!well) return;
    float bx, by;
    grid.ownerBase(9, bx, by);
    float x = bx + (well->minX + well->maxX) * 0.5f, y = by + well->minY - 1.0f;
    check(!ahead(world, x, y, 0, 0, false), "below the well is free");
    walk(world, x, y, 0, STEP, false);
    Blocker b = ahead(world, x, y, 0, 0.01f, false);
    check(b.kind == Blocker::Object && describe(area, b).find("Well") != std::string::npos,
          "walking up stops at the well (" + describe(area, b) + ")");
    world.setTileActive(9, false);
    b = ahead(world, x, y, 0, 0.01f, false);
    check(b.kind == Blocker::InactiveTile, "uncollected tile 9: its slot blocks instead (" + describe(area, b) + ")");
}

TEST(physics_npcs_and_items_block_collectibles_dont) {
    // NPCs have a big trigger (talk range) and a small solid box; items (cats, knot nodes) a solid
    // circle. The solid ones stop the player; collectibles are triggers only.
    const Area& area = village();
    World world(area);
    for (const STileDef& d : area.stiles) world.setTileActive(d.islandId, true);
    for (const char* name : {"Objects/Nemsy", "Objects/Knot/Knot Node (2)", "Objects/Mittens"}) {
        const ObjectCollider* c = findCollider(area, name);
        check(c != nullptr, std::string("has ") + name);
        if (!c) continue;
        float bx, by;
        world.objectBase(c->object, bx, by);
        float x = bx + (c->minX + c->maxX) * 0.5f, y = by + c->minY - 1.5f;
        check(!ahead(world, x, y, 0, 0, false), std::string("below ") + name + " is free");
        walk(world, x, y, 0, STEP, false);
        Blocker b = ahead(world, x, y, 0, 0.01f, false);
        check(b.kind == Blocker::Object && area.pathOf(area.objectColliders[b.index].object).find(name) != std::string::npos,
              std::string("walking up stops at ") + name + " (" + describe(area, b) + ")");
    }
    for (const ObjectCollider& c : area.objectColliders)
        check(area.pathOf(c.object).find("Collectible") == std::string::npos, "collectibles don't collide: " + area.pathOf(c.object));
}

TEST(physics_inactive_tile_is_solid) {
    // Walking from tile 8's slot (center 0,17, collected here) into tile 9's slot (center 17,17):
    // the slot's left edge blocks while tile 9 is uncollected, and the tile itself does not once it is.
    const Area& area = village();
    World world(area);
    Grid& grid = world.grid;
    world.setTileActive(8, true);
    const TileState* t9 = grid.tile(9);
    check(t9 && !t9->active, "tile 9 exists and starts inactive");
    if (!t9) return;
    const float edge = t9->posX * grid.tileWidth() - grid.tileWidth() * 0.5f;
    Blocker b = boxBlocker(world, probe(edge - 0.25f, 17.0f, 0.5f, 0.0f), false);
    check(b.kind == Blocker::InactiveTile && b.index == 9, "inactive tile 9 blocks (" + describe(area, b) + ")");
    check(!boxBlocked(world, probe(edge - 0.25f, 17.0f), false), "control: just left of the slot is free");
    world.setTileActive(9, true);
    b = boxBlocker(world, probe(edge - 0.25f, 17.0f, 0.5f, 0.0f), false);
    check(b.kind != Blocker::InactiveTile, "collected tile 9 no longer blocks as a slot (" + describe(area, b) + ")");
}

TEST(physics_raft_stops_at_the_coast) {
    // On water the land colliders apply: the raft's 1.5-wide box starts a little east of the coast,
    // so sailing west from the Beach spawn stops there; east is open sea. On foot the spawn point is
    // free too.
    const Area& area = village();
    World world(area);
    Grid& grid = world.grid;
    const Spawn* beach = area.spawn("VillageBeach");
    check(beach != nullptr, "VillageBeach spawn exists");
    if (!beach) return;
    float x, y;
    spawnPos(grid, *beach, x, y);
    const float startX = x;
    int frames = walk(world, x, y, -STEP, 0, true);
    Blocker b = ahead(world, x, y, -0.01f, 0, true);
    check(frames >= 1 && frames < 600 && b.kind == Blocker::Layer,
          "raft sails west " + std::to_string(startX - x) + " units, then is blocked by " + describe(area, b));
    spawnPos(grid, *beach, x, y);
    for (int i = 0; i < 30; i++) moveWithCollision(world, area.player.boatBox, x, y, STEP, 0, true);
    check(x - startX > 2.4f, "control: sailing east for half a second is free (" + std::to_string(x - startX) + ")");
    float fx, fy;
    spawnPos(grid, *beach, fx, fy);
    check(!boxBlocked(world, probe(fx, fy), false), "the Beach spawn point itself is not solid on foot");
}

TEST(physics_slide_fences_block_in_world_and_house) {
    const Area& area = village();
    World world(area);
    Grid& grid = world.grid;
    // Everything collected except 9 (the empty slot, middle), then slide tile 7 (2,1) left into it.
    for (const STileDef& d : area.stiles) world.setTileActive(d.islandId, true);
    world.setTileActive(9, false);
    Blocker before = boxBlocker(world, probe(25.0f, 7.0f, 0.0f, 3.0f), false);
    grid.startMove(SMove::swap(2, 1, 1, 1, 7, 9), 0, 1);
    // The move covers slots (1,1) and (2,1); their bottom edge is at y = 8.5.
    Blocker outside = boxBlocker(world, probe(25.0f, 7.0f, 0.0f, 3.0f), false);
    Blocker house = boxBlocker(world, probe(25.0f, 7.0f + grid.housingOffset(), 0.0f, 3.0f), false);
    check(before.kind != Blocker::Wall, "no fence before the slide (" + describe(area, before) + ")");
    check(outside.kind == Blocker::Wall, "fence blocks entering the slide (" + describe(area, outside) + ")");
    check(house.kind == Blocker::Wall, "fence blocks in the house copy too (" + describe(area, house) + ")");
    while (grid.moving()) grid.update(1.0f / 60.0f);
    check(grid.walls().empty(), "fences drop when the slide ends");
}

TEST(physics_overlapping_box_is_pushed_out_not_in) {
    // A box overlapping a wall (as after a slide) is pushed out, and walking further into the wall
    // doesn't get it any deeper.
    const Area& area = village();
    World world(area);
    Grid& grid = world.grid;
    const Spawn* def = area.spawn("Default");
    check(def != nullptr, "Default spawn exists");
    if (!def) return;
    float x, y;
    spawnPos(grid, *def, x, y);
    walk(world, x, y, STEP, 0, false); // flush against the wall on the right
    const float flushX = x;
    x += 0.2f; // now overlapping it
    check((bool)ahead(world, x, y, 0, 0, false), "starts overlapping the wall");
    for (int i = 0; i < 10; i++) moveWithCollision(world, area.player.footBox, x, y, STEP, 0, false);
    check(!ahead(world, x, y, 0, 0, false) && x <= flushX + 1e-3f,
          "walking into it ends free, not deeper (x " + std::to_string(x - flushX) + " past flush)");
}

TEST(physics_buried_box_walks_free) {
    // Buried deeper than a unit (the middle of an inactive slot), the box walks free rather than
    // being stuck forever.
    const Area& area = village();
    World world(area);
    Grid& grid = world.grid;
    const TileState* t9 = grid.tile(9);
    check(t9 != nullptr, "tile 9 exists");
    if (!t9) return;
    float x = t9->posX * grid.tileWidth(), y = t9->posY * grid.tileWidth();
    check((bool)ahead(world, x, y, 0, 0, false), "starts buried");
    const float sx = x;
    check(moveWithCollision(world, area.player.footBox, x, y, STEP, 0, false) && x > sx, "still moves");
}

TEST(physics_slides_around_round_colliders) {
    // Walking straight into a knot node (a circle) so the box's corner meets it (0.6 off center; the
    // box is 0.75 tall, so a flat face at a smaller offset stops dead, as in Box2D) slides around it; a flat wall still stops the player dead (house walls test). The lane is
    // chosen so it is clear with the knot hidden (the negative control).
    const Area& area = village();
    World world(area);
    Grid& grid = world.grid;
    for (const STileDef& d : area.stiles) world.setTileActive(d.islandId, true);
    const Hitbox& hb = area.player.footBox;
    int lanes = 0;
    for (const ObjectCollider& knot : area.objectColliders) {
        const std::string& path = area.pathOf(knot.object);
        if (knot.kind != ObjectCollider::Kind::Circle || path.find("Knot Node") == std::string::npos) continue;
        World without = world;
        without.objects.setActive(area, knot.object, false);
        float bx, by;
        grid.ownerBase(knot.owner, bx, by);
        const float cx = bx + knot.cx, cy = by + knot.cy - hb.offY;
        for (int dir : {1, -1}) {
            // Approach along x from 1.5 units away, 0.6 above center; the lane must be clear without the knot.
            const float sx = cx - dir * 1.5f, sy = cy + 0.6f;
            bool clear = true;
            for (float t = 0; t <= 3.0f && clear; t += 0.125f) // incl. the band the slide deflects into
                for (float lift : {0.0f, 0.4f, 0.8f})
                    clear = clear && !boxBlocked(without, hitboxAt(hb, sx + dir * t, sy + lift), false);
            if (!clear) continue;
            float x = sx, y = sy;
            for (int i = 0; i < 60; i++) moveWithCollision(world, hb, x, y, dir * STEP, 0, false);
            check(dir * (x - cx) > 0.5f, path + ": walked past it (" + std::to_string(dir * (x - cx)) + ")");
            check(!boxBlocked(world, hitboxAt(hb, x, y), false), path + ": ends free");
            lanes++;
        }
    }
    check(lanes >= 2, "found clear lanes past knot nodes (" + std::to_string(lanes) + ")");
}
