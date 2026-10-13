#include "game/conditions.h"

#include <algorithm>

#include "game/events.h"
#include "game/world.h"

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
