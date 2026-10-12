// Condition.cs / Conditionals / ConditionChecker: evaluating exported conditions against the game
// state, and running the start checks (ConditionChecker.checkOnStart) when an area loads.
#pragma once

#include <string>
#include <vector>

#include "game/area.h"
#include "game/grid.h"
#include "game/object_states.h"

// The save-profile state conditions read. A stand-in until the save profile exists (phase 4):
// flags only; collected Sliders are the Grid's active tiles.
struct GameState {
    std::vector<std::string> flags; // bools set to true

    bool flag(const std::string& name) const;
    void setFlag(const std::string& name, bool value);
};

// Condition.CheckCondition. Item: "Slider N" of the Village counts as collected when tile N is
// active; other collectibles aren't tracked yet (false). PlayerCarryingItem and unsupported
// (spec) conditions are false; GridStationary needs no slide in progress at all.
bool checkCondition(const ConditionDef& c, const GameState& state, const Grid& grid);

// Every start check whose object is active: all conditions true -> its onSuccess SetActive calls,
// else onFail, in export order. Run once after loading (or after changing the state on purpose).
void runStartChecks(const Area& area, const GameState& state, const Grid& grid, ObjectStates& objects);
