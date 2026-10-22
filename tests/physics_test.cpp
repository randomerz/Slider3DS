// Collision against the real exported Village. Every "blocked" check has a matching "free"
// control, so a world that turned fully solid (or fully open) fails.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "game/data/area.h"
#include "game/grid/grid.h"
#include "game/physics/geometry.h"
#include "game/physics/physics.h"
#include "game/world/world.h"
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
    const Area& area = villageArea();
    const Hitbox& f = area.player.footBox;
    const Hitbox& b = area.player.boatBox;
    check(f.w == 0.75f && f.h == 0.75f && f.offY == -0.0625f, "foot box: 0.5x0.5 + 0.125 edge radius at (0,-1/16)");
    check(b.w == 1.5f && b.h == 0.75f && b.offY == -0.375f, "boat box: 1.5x0.75 at (0,-0.375)");
}

TEST(physics_spawns_start_free) {
    // Spawns on tiles that aren't collected yet (e.g. VillageWaterfall on tile 6) are only used
    // once they are, so check with every tile active.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
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
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
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
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
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
    const Area& area = villageArea();
    check(area.objectColliders.size() > 150, "village has object colliders (" + std::to_string(area.objectColliders.size()) + ")");
    World world(area, DEFAULT_SETTINGS);
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
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
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
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
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
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
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
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
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
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
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
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
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
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
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

TEST(physics_geometry_touching_is_not_overlap) {
    // Exact boundary cases of the shape tests (in = the unit box's interior): touching is free,
    // and a hair further in is a hit.
    using namespace geom;
    const Box in = {0, 0, 1, 1};
    CHECK(!segmentHitsBox(2, 0.5f, 1, 0.5f, in));   // ends on the right face
    CHECK(segmentHitsBox(2, 0.5f, 0.9f, 0.5f, in)); // ...just inside
    CHECK(!segmentHitsBox(0, 2, 2, 0, in));         // diagonal through the corner (1, 1)
    CHECK(segmentHitsBox(0, 1.9f, 1.9f, 0, in));
    CHECK(!segmentHitsBox(1, -1, 1, 2, in));        // along the face
    const float flush[] = {1, 0, 2, 0, 2, 1, 1, 1}, into[] = {0.9f, 0, 2, 0, 2, 1, 0.9f, 1};
    CHECK(!polygonHitsBox(flush, 4, 0, 0, in));
    CHECK(polygonHitsBox(into, 4, 0, 0, in));
    const float around[] = {-1, -1, 2, -1, 2, 2, -1, 2};
    CHECK(polygonHitsBox(around, 4, 0, 0, in));     // contains the box: no edge crosses it
    CHECK(!circleHitsBox(2, 0.5f, 1, in));
    CHECK(circleHitsBox(2, 0.5f, 1.01f, in));
    CHECK(!boxesOverlap(in, {1, 0, 2, 1}) && boxesOverlap(in, {0.99f, 0, 2, 1}));
}

namespace {

// Independent reference for boxBlocked: every fence, inactive slot, collision-map shape and object
// collider, no broadphase or culling.
bool blockedByAnything(const World& world, const Box& box, bool onWater) {
    using namespace geom;
    const Area& area = world.area;
    const Grid& grid = world.grid;
    const Box in = inner(box);
    for (const Wall& w : grid.walls())
        if (segmentHitsBox(w.x0, w.y0, w.x1, w.y1, in)) return true;
    auto collides = [&](int owner) {
        const TileState* t = owner ? grid.tile(owner) : nullptr;
        return owner == 0 || (t && t->active && t->collidersOff <= 0);
    };
    const float half = grid.tileWidth() * 0.5f;
    for (const TileState& t : grid.tiles()) {
        if (t.active || t.collidersOff > 0) continue;
        const float cx = t.posX * grid.tileWidth(), cy = t.posY * grid.tileWidth(), hy = cy + grid.housingOffset();
        if (boxesOverlap(in, {cx - half, cy - half, cx + half, cy + half}) ||
            boxesOverlap(in, {cx - half, hy - half, cx + half, hy + half}))
            return true;
    }
    for (const CollisionLayer& L : area.collision) {
        if ((L.mode == CollisionMode::OnFoot && onWater) || (L.mode == CollisionMode::OnWater && !onWater) ||
            (L.mode == CollisionMode::Always && !L.active) || !collides(L.owner))
            continue;
        float bx, by;
        grid.ownerBase(L.owner, bx, by);
        const float ox = bx + L.originX + L.minX, oy = by + L.originY + L.minY;
        for (int cy = 0; cy < L.h; cy++)
            for (int cx = 0; cx < L.w; cx++) {
                const int shape = L.cell(cx, cy);
                if (shape < 0) continue;
                for (const Polygon& p : area.shapes[shape])
                    if (polygonHitsBox(p.data(), p.size() / 2, ox + cx, oy + cy, in)) return true;
            }
    }
    for (const ObjectCollider& c : area.objectColliders)
        if (colliderOverlaps(world, c, box)) return true;
    return false;
}

} // namespace

TEST(physics_box_blocker_matches_testing_everything) {
    // The broadphase (owner bounds, cell ranges, shape bounds, moved objects) must not change an
    // answer: boxBlocked vs testing every shape, mid-slide with the player riding tile 9 (so tile 7,
    // the other tile in the move, has its colliders off, its objects' too, also one placed onto it
    // since), an inactive slot, a
    // disabled collider; on foot and on water; then again once the slide is over.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    for (const STileDef& d : area.stiles) world.setTileActive(d.islandId, true);
    world.setTileActive(5, false);
    world.grid.startMove(SMove::swap(2, 1, 1, 1, 7, 9), 9, 1);
    for (int i = 0; i < 20; i++) world.grid.update(1.0f / 60.0f);
    check(world.grid.tile(7)->collidersOff > 0, "tile 7 slides with its colliders off");
    // An item with a solid collider, placed onto the sliding tile.
    int item = -1;
    for (const ItemDef& it : area.items)
        if (area.objectColliderStart[it.object + 1] > area.objectColliderStart[it.object]) item = it.object;
    check(item >= 0, "an item with a solid collider");
    float tx, ty;
    world.grid.ownerBase(7, tx, ty);
    world.placeObject(item, 7, tx + 2.5f, ty - 1.5f);
    world.objects.setColliderEnabled(area.objectColliders[area.objectCollidersOf[0]].slot, false);

    unsigned rng = 11;
    auto rnd = [&](float lo, float hi) {
        rng = rng * 1103515245u + 12345u;
        return lo + (hi - lo) * ((rng >> 8) & 0xffff) / 65535.0f;
    };
    auto compare = [&](const char* phase, int& blocked, int& onSlidingFree) {
        int mismatches = 0;
        auto one = [&](const Box& b, bool onWater, bool onSliding) {
            const bool want = blockedByAnything(world, b, onWater), got = boxBlocked(world, b, onWater);
            blocked += want;
            onSlidingFree += onSliding && !want;
            if (want != got && mismatches++ < 5)
                check(false, std::string(phase) + ": box " + std::to_string(b.minX) + "," + std::to_string(b.minY) +
                                 (onWater ? " on water" : "") + ": every shape says " + (want ? "blocked" : "free"));
        };
        // A small box on each object collider, where it is now.
        for (const ObjectCollider& c : area.objectColliders) {
            float bx, by;
            world.objectBase(c.object, bx, by);
            const float x = bx + (c.minX + c.maxX) * 0.5f, y = by + (c.minY + c.maxY) * 0.5f;
            one({x - 0.15f, y - 0.15f, x + 0.15f, y + 0.15f}, false, world.objects.owner(c.object) == 7);
        }
        // Random boxes over the grid and the house interiors.
        for (int k = 0; k < 300; k++) {
            const float w = rnd(0.2f, 1.5f), h = rnd(0.2f, 1.5f);
            const float x = rnd(-10, 60), y = k % 4 == 0 ? rnd(-165, -120) : rnd(-10, 60);
            one({x, y, x + w, y + h}, k % 3 == 0, false);
        }
        check(mismatches == 0, std::string(phase) + ": " + std::to_string(mismatches) + " mismatches");
    };
    int blocked = 0, slidingFree = 0;
    compare("mid-slide", blocked, slidingFree);
    check(blocked > 100 && slidingFree > 3, "mid-slide covers blocked boxes and the sliding tile's colliders (" +
                                                 std::to_string(blocked) + ", " + std::to_string(slidingFree) + ")");
    for (int i = 0; i < 600 && world.grid.tile(7)->collidersOff > 0; i++) world.grid.update(1.0f / 60.0f);
    check(world.grid.tile(7)->collidersOff == 0, "the slide ended");
    int blockedAfter = 0, slidingFreeAfter = 0;
    compare("after the slide", blockedAfter, slidingFreeAfter);
    check(slidingFreeAfter < slidingFree, "tile 7's colliders block again");
}

TEST(physics_collision_near_answers_like_box_blocker) {
    // CollisionNear (a move's gathered geometry) must give boxBlocker's answer for every box: inside
    // its region from the short list, outside (or too crowded) from the full query. Mid-slide (fences,
    // a tile with colliders off), with a moved item and an inactive slot, on foot and on water.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    for (const STileDef& d : area.stiles) world.setTileActive(d.islandId, true);
    world.setTileActive(9, false);
    world.setTileActive(5, false);
    world.grid.startMove(SMove::swap(2, 1, 1, 1, 7, 9), 0, 1);
    for (int i = 0; i < 20; i++) world.grid.update(1.0f / 60.0f);
    check(!world.grid.walls().empty(), "mid-slide");
    check(!area.items.empty(), "the village has items");
    float ix, iy;
    world.objectPosition(area.items[0].object, ix, iy);
    world.placeObject(area.items[0].object, 0, ix + 0.7f, iy - 0.3f);
    check(!world.objects.moved().empty(), "an item moved");

    unsigned rng = 7;
    auto rnd = [&](float lo, float hi) {
        rng = rng * 1103515245u + 12345u;
        return lo + (hi - lo) * ((rng >> 8) & 0xffff) / 65535.0f;
    };
    int regions = 0, inside = 0, blocked = 0, free = 0, mismatches = 0;
    for (int r = 0; r < 400; r++) {
        const bool onWater = r % 3 == 0;
        // Around the moved item for some, anywhere over the grid for the rest; now and then a big one.
        const float cx = r % 8 == 0 ? ix + 0.7f : rnd(-10, 60), cy = r % 8 == 0 ? iy - 0.3f : rnd(-10, 60);
        const float half = r % 25 == 0 ? 12.0f : rnd(0.5f, 2.0f);
        const Box region = {cx - half, cy - half, cx + half, cy + half};
        const CollisionNear near(world, region, onWater);
        regions++;
        for (int k = 0; k < 40; k++) {
            // Mostly inside the region; some straddle its edge (the fallback).
            const float w = rnd(0.05f, 1.0f), h = rnd(0.05f, 1.0f);
            const float x = k % 5 == 0 ? rnd(region.minX - 1, region.maxX) : rnd(region.minX, region.maxX - w);
            const float y = k % 5 == 0 ? rnd(region.minY - 1, region.maxY) : rnd(region.minY, region.maxY - h);
            const Box b = {x, y, x + w, y + h};
            const Blocker want = boxBlocker(world, b, onWater), got = near.blocker(b);
            if (b.minX >= region.minX && b.maxX <= region.maxX && b.minY >= region.minY && b.maxY <= region.maxY) inside++;
            (want ? blocked : free)++;
            if (want.kind != got.kind || want.index != got.index) {
                if (mismatches++ < 5)
                    check(false, "box " + std::to_string(x) + "," + std::to_string(y) + ": " + describe(area, want) +
                                     " vs " + describe(area, got));
            }
        }
    }
    check(mismatches == 0, std::to_string(mismatches) + " mismatches");
    check(inside > 8000 && blocked > 1000 && free > 1000,
          "covers both answers (" + std::to_string(blocked) + " blocked, " + std::to_string(free) + " free, " +
              std::to_string(inside) + " inside)");
}

TEST(physics_moved_objects_follow_placements) {
    // ObjectStates::moved() (kept up to date as placements change) is exactly the objects not where
    // they were exported: physics checks their colliders on their own.
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    auto expected = [&] {
        std::vector<int> v;
        for (int i = 0; i < (int)area.objects.size(); i++)
            if (world.objects.isMoved(area, i)) v.push_back(i);
        return v;
    };
    check(world.objects.moved() == expected(), "at the start (" + std::to_string(expected().size()) + " moved by Starts)");
    float x, y;
    const int item = area.items[0].object;
    world.objectPosition(item, x, y);
    world.placeObject(item, 0, x + 1, y);
    check(world.objects.moved() == expected() && !world.objects.moved().empty(), "an item placed elsewhere (and what's under it)");
    int npc = -1, target = -1;
    for (int i = 0; i < (int)area.objects.size() && (npc < 0 || target < 0); i++)
        if (area.objects[i].parent < 0 && i != item) (npc < 0 ? npc : target) = i;
    world.objects.moveOnto(area, npc, target);
    check(world.objects.moved() == expected(), "an object moved onto another");
    world.placeObject(item, area.objects[item].owner, x, y);
    check(world.objects.moved() == expected(), "the item put back");
    world.objects.setPlacement(area, item, -1, 0, 0);
    check(world.objects.moved() == expected(), "its placement cleared");
    world.objects.init(area);
    check(world.objects.moved().empty() && expected().empty(), "init: everything where it was exported");
}

TEST(physics_scripted_walk_is_pinned) {
    // A fixed walk from every spawn (tiles 2-9 collected; pseudo-random directions, the action button
    // now and then: pick-ups, drops, the raft) hashed frame by frame. It pins the player's movement
    // across physics refactors: the broadphase and CollisionNear must move the player exactly as a
    // test of every shape does. (It caught a touch-only segment counting as a hit before the cull,
    // which left the raft stuck on the east edge.) An intended movement change re-pins the hash (as
    // NPC walks did: walking NPCs' colliders move).
    const Area& area = villageArea();
    World world(area, DEFAULT_SETTINGS);
    for (int t = 2; t <= 9; t++) world.setTileActive(t, true);
    world.state.setFlag("villageIntroCutscene", true); // (seen: leaving the house doesn't stop the player)
    unsigned long long h = 1469598103934665603ull; // FNV-1a over the positions' bits
    auto mix = [&](float f) { unsigned u; std::memcpy(&u, &f, 4); h = (h ^ u) * 1099511628211ull; };
    unsigned rng = 12345;
    int exits = 0;
    for (size_t sp = 0; sp < area.spawns.size(); sp++) {
        world.placeAt(area.spawns[sp]);
        for (int seg = 0; seg < 40; seg++) {
            rng = rng * 1103515245 + 12345;
            const int dir = (rng >> 16) % 9;
            const float mx = (float)(dir % 3) - 1, my = (float)(dir / 3) - 1;
            for (int f = 0; f < 45; f++) {
                world.update(1.0f / 60, mx, my, false);
                if (world.playerLocked()) { // (walked into an exit: let go of it, and of its fade)
                    exits++;
                    world.lockPlayer(LOCK_SCENE, false);
                    world.effects.reset();
                }
                mix(world.player.x);
                mix(world.player.y);
            }
            if (seg % 7 == 3) world.action();
        }
    }
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", h);
    check(std::string(hex) == "7ba099e76c59be07", std::string("walk hash ") + hex);
    CHECK(exits == 10); // (walks reach exits: the beach's and the Waterfall's)
}
