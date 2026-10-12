#include "game/conditions.h"

#include <algorithm>
#include <cstdlib>

bool GameState::flag(const std::string& name) const {
    return std::find(flags.begin(), flags.end(), name) != flags.end();
}

void GameState::setFlag(const std::string& name, bool value) {
    auto it = std::find(flags.begin(), flags.end(), name);
    if (value && it == flags.end()) flags.push_back(name);
    if (!value && it != flags.end()) flags.erase(it);
}

bool checkCondition(const ConditionDef& c, const GameState& state, const Grid& grid) {
    using T = ConditionDef::Type;
    switch (c.type) {
    case T::Item: {
        const std::string prefix = "Slider ";
        if (c.itemArea != "Village" || c.name.compare(0, prefix.size(), prefix) != 0) return false;
        const int id = std::atoi(c.name.c_str() + prefix.size());
        return id > 0 && grid.tileActive(id);
    }
    case T::Grid: return CheckGrid::contains(grid.gridString(), c.pattern);
    case T::NoGrid: return !CheckGrid::contains(grid.gridString(), c.pattern);
    case T::GridStationary: return CheckGrid::contains(grid.gridString(), c.pattern) && !grid.moving();
    case T::Flag: return state.flag(c.name);
    case T::NoFlag: return !state.flag(c.name);
    case T::PlayerCarryingItem:
    case T::Unsupported: return false;
    }
    return false;
}

void runStartChecks(const Area& area, const GameState& state, const Grid& grid, ObjectStates& objects) {
    for (const StartCheck& sc : area.startChecks) {
        if (!sc.active) continue;
        bool ok = true;
        for (const ConditionDef& c : sc.conditions)
            if (!checkCondition(c, state, grid)) {
                ok = false;
                break;
            }
        for (const SetActiveCall& call : ok ? sc.onSuccess : sc.onFail) objects.setActive(area, call.target, call.active);
    }
}
