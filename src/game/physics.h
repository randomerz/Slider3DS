// Player collision against an Area, mirroring Player.FixedUpdate in the original game:
// the player is a point that raycasts along its move; when blocked it tries the X part of the
// move alone, then the Y part alone (so it slides along walls).
#pragma once

#include <string>

#include "game/area.h"

// True if the segment (x, y) -> (x + dx, y + dy) hits solid collision, including a start point
// inside a collider (Unity's queriesStartInColliders). Collision maps follow the water state;
// inactive STiles are solid over their whole slot and their house interior; Area::borderWalls
// (fences around sliding tiles) block too.
bool segmentBlocked(const Area& area, float x, float y, float dx, float dy, bool onWater);

// Same test, returning what blocks it (a collision tilemap path or "inactive STile N"), or ""
// if nothing does. For debugging and tests.
std::string segmentBlockedBy(const Area& area, float x, float y, float dx, float dy, bool onWater);

// Moves (x, y) by (dx, dy) with the original's fallback order. Returns true if it moved.
bool moveWithCollision(const Area& area, float& x, float& y, float dx, float dy, bool onWater);
