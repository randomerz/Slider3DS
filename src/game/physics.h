// Player collision against an Area, mirroring the original's physics: the player is a dynamic
// Rigidbody2D whose box collider (Player.colliderPlayerVers on foot, colliderBoatVers on water)
// Box2D pushes out of every solid collider on a layer the Player layer collides with: tilemap
// walls, props, NPCs, items, inactive STile slots, slide fences. (Player.FixedUpdate also raycasts
// its position point against the Default layer, but the box always meets a collider first.)
// So the port moves that box: along X, then along Y, each stopping flush against what it hits,
// which slides along walls the way the push-out does (see moveWithCollision).
#pragma once

#include <string>

#include "game/area.h"
#include "game/grid.h"
#include "game/object_states.h"

// What overlaps a box. kind None = nothing does.
struct Blocker {
    enum Kind { None, Wall, InactiveTile, Layer, Object } kind = None;
    // Wall: index in grid.walls(); InactiveTile: islandId; Layer: index in area.collision;
    // Object: index in area.objectColliders
    int index = 0;
    explicit operator bool() const { return kind != None; }
};

// An axis-aligned box in world units.
struct Box {
    float minX, minY, maxX, maxY;
};

// `hb` placed at player position (x, y).
inline Box hitboxAt(const Hitbox& hb, float x, float y) {
    const float cx = x + hb.offX, cy = y + hb.offY;
    return {cx - hb.w * 0.5f, cy - hb.h * 0.5f, cx + hb.w * 0.5f, cy + hb.h * 0.5f};
}

// What overlaps the box's interior (touching is fine). Collision maps follow the water state and
// the owning tile's position; inactive STiles are solid over their whole slot and their house
// interior; the grid's slide fences and the active object colliders (`objects`) block too. Tiles
// with colliders switched off (Grid: sliding in a move the player isn't riding) are skipped, with
// their objects.
Blocker boxBlocker(const Area& area, const Grid& grid, const ObjectStates& objects, const Box& box, bool onWater);

inline bool boxBlocked(const Area& area, const Grid& grid, const ObjectStates& objects, const Box& box, bool onWater) {
    return (bool)boxBlocker(area, grid, objects, box, onWater);
}

// Human-readable blocker ("slide border", "inactive STile 9", "STile 1 <tilemap path>",
// "object <path>"), for debugging and tests.
std::string describe(const Area& area, const Blocker& b);

// Moves the player at (x, y) with hitbox `hb` by (dx, dy): X first, then Y, each as far as it goes
// before touching a collider; a single-axis step blocked by a round or slanted collider slides
// around it. A box that already overlaps something (a tile slid onto the player) is first pushed
// out to the nearest free spot, as Box2D would; one buried deeper than a unit walks free. Returns
// true if it moved.
bool moveWithCollision(const Area& area, const Grid& grid, const ObjectStates& objects, const Hitbox& hb, float& x,
                       float& y, float dx, float dy, bool onWater);
