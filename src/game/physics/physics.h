// Player collision against an Area, mirroring the original's physics: the player is a dynamic
// Rigidbody2D whose box collider (Player.colliderPlayerVers on foot, colliderBoatVers on water)
// Box2D pushes out of every solid collider on a layer the Player layer collides with: tilemap
// walls, props, NPCs, items, inactive STile slots, slide fences. (Player.FixedUpdate also raycasts
// its position point against the Default layer, but the box always meets a collider first.)
// So the port moves that box: along X, then along Y, each stopping flush against what it hits,
// which slides along walls the way the push-out does (see moveWithCollision).
#pragma once

#include <string>

#include "game/data/area.h"
#include "game/physics/geometry.h"

class World;

// What overlaps a box. kind None = nothing does.
struct Blocker {
    enum Kind { None, Wall, InactiveTile, Layer, Object } kind = None;
    // Wall: index in grid.walls(); InactiveTile: islandId; Layer: index in area.collision;
    // Object: index in area.objectColliders
    int index = 0;
    explicit operator bool() const { return kind != None; }
};

struct Wall;

// A piece of solid geometry near a query (CollisionNear): one slide fence, inactive-slot box,
// collision-map polygon (at its cell) or object collider (at its base).
// (No member initializers of its own, so CollisionNear's array costs next to nothing until filled.)
struct NearShape {
    Blocker blocker;                // what it is
    Box bounds;                     // in world units
    const Wall* wall;               // Wall
    const float* pts;               // Layer: the polygon's n points, offset by (ox, oy)
    int n;
    const ObjectCollider* collider; // Object: offset by (ox, oy)
    float ox, oy;
};

// `hb` placed at player position (x, y).
inline Box hitboxAt(const Hitbox& hb, float x, float y) {
    const float cx = x + hb.offX, cy = y + hb.offY;
    return {cx - hb.w * 0.5f, cy - hb.h * 0.5f, cx + hb.w * 0.5f, cy + hb.h * 0.5f};
}

// What overlaps the box's interior (touching is fine). Collision maps follow the water state and
// the owning tile's position; inactive STiles are solid over their whole slot and their house
// interior; the grid's slide fences and the active object colliders block too. Tiles
// with colliders switched off (Grid: sliding in a move the player isn't riding) are skipped, with
// their objects.
// `ignoreCollider` (an Area::objectColliders index) is left out (e.g. an item checking its own way).
Blocker boxBlocker(const World& world, const Box& box, bool onWater, int ignoreCollider = -1);

inline bool boxBlocked(const World& world, const Box& box, bool onWater, int ignoreCollider = -1) {
    return (bool)boxBlocker(world, box, onWater, ignoreCollider);
}

// The solid geometry near a region, gathered once for a run of queries inside it (a move's sweeps
// and probes, an item's drop search): blocker() answers as boxBlocker does, from a short list.
// Boxes not inside the region (or a region with too much in it) take the full query.
// It points into the World (tile positions, slide fences, object placements as of the gather):
// use it only until the grid or objects change, never keep one across updates.
class CollisionNear {
public:
    CollisionNear(const World& world, const Box& region, bool onWater, int ignoreCollider = -1);
    Blocker blocker(const Box& box) const;
    bool blocked(const Box& box) const { return (bool)blocker(box); }

private:
    static constexpr int CAPACITY = 64;
    const World& world_;
    Box region_;
    bool onWater_;
    int ignoreCollider_;
    NearShape shapes_[CAPACITY];
    int count_ = 0;
    bool overflow_ = false;
};

// Whether collider `c` (solid or trigger) is on (its object active, its tile collected and not
// sliding without the player) and overlaps the box's interior.
bool colliderOverlaps(const World& world, const ObjectCollider& c, const Box& box);

// Human-readable blocker ("slide border", "inactive STile 9", "STile 1 <tilemap path>",
// "object <path>"), for debugging and tests.
std::string describe(const Area& area, const Blocker& b);

// Moves the player at (x, y) with hitbox `hb` by (dx, dy): X first, then Y, each as far as it goes
// before touching a collider; a single-axis step blocked by a round or slanted collider slides
// around it. A box that already overlaps something (a tile slid onto the player) is first pushed
// out to the nearest free spot, as Box2D would, also when (dx, dy) is zero; one buried deeper than
// the push-out reaches (4 units) walks free. Returns true if it moved.
bool moveWithCollision(const World& world, const Hitbox& hb, float& x, float& y, float dx, float dy, bool onWater);

// The nearest point to (x, y) where hitbox `hb` (on foot) is free, searching outward in half-unit
// rings up to 8 units; false (and x, y unchanged) if there is none.
bool findFreeNear(const World& world, const Hitbox& hb, float& x, float& y);
