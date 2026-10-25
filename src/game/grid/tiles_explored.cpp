#include "game/grid/tiles_explored.h"

#include <algorithm>
#include <cstdio>

std::string TilesExplored::key(const std::string& area, int islandId) {
    char id[8];
    std::snprintf(id, sizeof id, "%02d", islandId);
    return "TilesExplored_" + area + "_" + id;
}

void TilesExplored::reset(const Area& area, const GameState& state) {
    area_ = area.name;
    int maxId = 0;
    for (const STileDef& s : area.stiles) maxId = std::max(maxId, s.islandId);
    explored_.assign(maxId + 1, 0);
    for (const STileDef& s : area.stiles) explored_[s.islandId] = state.flag(key(area_, s.islandId));
    last_ = 0;
}

void TilesExplored::update(int stileUnderPlayer, GameState& state) {
    if (stileUnderPlayer <= 0 || stileUnderPlayer == last_ || stileUnderPlayer >= (int)explored_.size()) return;
    last_ = stileUnderPlayer;
    if (explored_[last_]) return;
    explored_[last_] = 1;
    state.setFlag(key(area_, last_), true);
}

bool TilesExplored::explored(int islandId) const {
    return islandId > 0 && islandId < (int)explored_.size() && explored_[islandId];
}
