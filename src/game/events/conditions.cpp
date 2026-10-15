#include "game/events/conditions.h"

#include <algorithm>

#include "game/events/events.h"
#include "game/village/village_grid.h"
#include "game/world/world.h"

bool checkCondition(const ConditionDef& c, const World& world) {
    using T = ConditionDef::Type;
    const GameState& state = world.state;
    const Grid& grid = world.grid;
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
    case T::Spec: return village::checkSpec(world, c.spec, c.specTarget);
    case T::PlayerCarryingItem: return world.items.carrying(world.area, c.name);
    case T::Unsupported: return false;
    }
    return false;
}

bool runConditionals(World& world, const ConditionalsDef& c) {
    const bool ok = std::all_of(c.conditions.begin(), c.conditions.end(),
                                [&](const ConditionDef& d) { return checkCondition(d, world); });
    runCalls(world, ok ? c.onSuccess : c.onFail);
    return ok;
}

bool conditionReadsOnlyStateAndGrid(const ConditionDef& c) {
    using T = ConditionDef::Type;
    switch (c.type) {
    case T::Item:
    case T::Grid:
    case T::GridStationary:
    case T::NoGrid:
    case T::Flag:
    case T::NoFlag: return true;
    case T::Spec: return village::specReadsOnlyStateAndGrid(c.spec);
    case T::PlayerCarryingItem:
    case T::Unsupported: return false;
    }
    return false;
}
