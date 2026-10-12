// Player collision against an Area, mirroring Player.FixedUpdate in the original game:
// the player is a point that raycasts along its move; when blocked it tries the X part of the
// move alone, then the Y part alone (so it slides along walls).
#pragma once

#include <string>

#include "game/area.h"
#include "game/grid.h"

// What stopped a segment. kind None = nothing did.
struct Blocker {
    enum Kind { None, Wall, InactiveTile, Layer } kind = None;
    int index = 0; // Wall: index in grid.walls(); InactiveTile: islandId; Layer: index in area.collision
    explicit operator bool() const { return kind != None; }
};

// What blocks the segment (x, y) -> (x + dx, y + dy), counting a start point inside a collider
// (Unity's queriesStartInColliders). Collision maps follow the water state and the owning tile's
// position; inactive STiles are solid over their whole slot and their house interior; the grid's
// slide fences (walls) block too. Tiles with colliders switched off (Grid) are skipped.
Blocker segmentBlocker(const Area& area, const Grid& grid, float x, float y, float dx, float dy, bool onWater);

inline bool segmentBlocked(const Area& area, const Grid& grid, float x, float y, float dx, float dy, bool onWater) {
    return (bool)segmentBlocker(area, grid, x, y, dx, dy, onWater);
}

// Human-readable blocker ("slide border", "inactive STile 9", "STile 1 <tilemap path>"), for
// debugging and tests.
std::string describe(const Area& area, const Blocker& b);

// Moves (x, y) by (dx, dy) with the original's fallback order. Returns true if it moved.
bool moveWithCollision(const Area& area, const Grid& grid, float& x, float& y, float dx, float dy, bool onWater);
