// SGridTilesExplored, part of World: which tiles the player has stood on, kept in the save profile
// as the original's keys ("TilesExplored_<Area>_<id, 2 digits>"). The Artifact's buttons flicker
// until their tile is explored (ArtifactUI).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/data/area.h"
#include "game/world/game_state.h"

class TilesExplored {
public:
    void reset(const Area& area, const GameState& state); // a scene load: read from the save
    void update(int stileUnderPlayer, GameState& state);  // SGridTilesExplored.Update (0 = no tile)
    bool explored(int islandId) const;

    static std::string key(const std::string& area, int islandId);

private:
    std::string area_;
    std::vector<uint8_t> explored_; // per islandId
    int last_ = 0;                  // lastStileUnderPlayer
};
