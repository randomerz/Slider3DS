// The player touching trigger colliders (Unity's OnTriggerEnter2D / OnTriggerExit2D, sent to the
// trigger's object), and what the Village's trigger behaviours do then:
//   - OnTriggerEnter: its onEnter / onExit calls (if onPlayerEnter);
//   - PlayerConditionals: offered as an interaction while inside (addToOnAction, unless excluded
//     on water / land), or run on entering;
//   - Collectible: picked up on entering (added to the GameState, hidden, onCollect run).
// A trigger that turns off (object hidden, tile sliding) while the player is inside counts as an
// exit, and so does the player's collider changing (onWater: foot <-> boat), followed by enters.
#pragma once

#include <cstdint>
#include <vector>

#include "game/area.h"

class World;

class TriggerTracker {
public:
    void reset(const Area& area); // touching nothing
    // Finds the objects whose active trigger colliders overlap the player's box, then sends the
    // exits and enters since the last update.
    void update(World& world);
    bool touching(int object) const { return touching_[object] != 0; }

private:
    std::vector<uint8_t> touching_, now_; // per Area::objects index
    int lastOnWater_ = -1;                // the player's collider at the last update (-1 = none yet)
};
