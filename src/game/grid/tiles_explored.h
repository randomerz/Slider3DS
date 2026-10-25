// SGridTilesExplored, part of World: which tiles the player has stood on, kept in the save profile
// as the original's keys ("TilesExplored_<Area>_<id, 2 digits>"), and since which World::frame each
// tile has been active. The Artifact's buttons flicker until their tile is explored (ArtifactUI,
// from that frame: the clock runs while the player is held, stops while paused).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/data/area.h"
#include "game/grid/grid.h"
#include "game/world/game_state.h"

class TilesExplored {
public:
    void reset(const Area& area, const GameState& state); // a scene load: read from the save
    // SGridTilesExplored.Update (`stileUnderPlayer` 0 = no tile); notes tiles turning active.
    void update(const Grid& grid, int stileUnderPlayer, uint32_t frame, GameState& state);
    bool explored(int islandId) const;
    int activeSince(int islandId) const; // the frame it was first seen active since it last wasn't, -1 = not active

    static std::string key(const std::string& area, int islandId);

private:
    std::string area_;
    std::vector<uint8_t> explored_; // per islandId
    std::vector<int> activeSince_;  // per islandId
    int last_ = 0;                  // lastStileUnderPlayer
};
