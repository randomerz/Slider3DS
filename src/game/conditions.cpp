#include "game/conditions.h"

#include <algorithm>

#include "game/events.h"
#include "game/world.h"

namespace {

void setIn(std::vector<std::string>& set, const std::string& key, bool present) {
    auto it = std::find(set.begin(), set.end(), key);
    if (present && it == set.end()) set.push_back(key);
    if (!present && it != set.end()) set.erase(it);
}

bool contains(const std::vector<std::string>& set, const std::string& key) {
    return std::find(set.begin(), set.end(), key) != set.end();
}

} // namespace

bool GameState::flag(const std::string& name) const { return contains(flags_, name); }
void GameState::setFlag(const std::string& name, bool value) { setIn(flags_, name, value); }

bool GameState::hasCollectible(const std::string& name, const std::string& area) const {
    return contains(collectibles_, area + "/" + name);
}

void GameState::setCollectible(const std::string& name, const std::string& area, bool collected) {
    setIn(collectibles_, area + "/" + name, collected);
}

bool checkCondition(const ConditionDef& c, const GameState& state, const Grid& grid) {
    using T = ConditionDef::Type;
    switch (c.type) {
    case T::Item: return state.hasCollectible(c.name, c.itemArea);
    case T::Grid: return CheckGrid::contains(grid.gridString(), c.pattern);
    case T::NoGrid: return !CheckGrid::contains(grid.gridString(), c.pattern);
    case T::GridStationary:
        if (!CheckGrid::contains(grid.gridString(), c.pattern)) return false;
        for (int id : c.stationaryTiles)
            if (grid.tileMoving(id)) return false;
        return true;
    case T::Flag: return state.flag(c.name);
    case T::NoFlag: return !state.flag(c.name);
    case T::PlayerCarryingItem:
    case T::Unsupported: return false;
    }
    return false;
}

bool runConditionals(World& world, const ConditionalsDef& c) {
    const bool ok = std::all_of(c.conditions.begin(), c.conditions.end(),
                                [&](const ConditionDef& d) { return checkCondition(d, world.state, world.grid); });
    runCalls(world, ok ? c.onSuccess : c.onFail);
    return ok;
}
