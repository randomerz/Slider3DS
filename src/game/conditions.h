// Condition.cs / Conditionals / ConditionChecker: evaluating exported conditions against the game
// state, and running start checks (ConditionChecker.checkOnStart) as their objects activate.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/area.h"
#include "game/grid.h"
#include "game/object_states.h"

// The save-profile state conditions read: bool flags (SaveProfile.GetBool) and collected
// collectibles (PlayerInventory.Contains). A stand-in with the same queries until the save
// profile exists (phase 4).
class GameState {
public:
    bool flag(const std::string& name) const;
    void setFlag(const std::string& name, bool value);
    bool hasCollectible(const std::string& name, const std::string& area) const;
    void setCollectible(const std::string& name, const std::string& area, bool collected);

private:
    std::vector<std::string> flags_;        // set to true
    std::vector<std::string> collectibles_; // "<area>/<name>"
};

// Condition.CheckCondition. PlayerCarryingItem and unsupported (spec) conditions are false until
// items and specs exist.
bool checkCondition(const ConditionDef& c, const GameState& state, const Grid& grid);

// Conditionals.CheckConditions: all conditions true -> onSuccess's SetActive calls, else onFail's.
// Returns whether they all held.
bool runConditionals(const Area& area, const ConditionalsDef& c, const GameState& state, const Grid& grid,
                     ObjectStates& objects);

// Unity calls a ConditionChecker's Start once, the first time its object is active (and the
// component enabled). update() runs every start check that hasn't started and now is active, in
// export order, until none are left (a check may activate another checker's object).
class StartChecks {
public:
    void reset(const Area& area); // none started (a fresh scene load)
    void update(const Area& area, const GameState& state, const Grid& grid, ObjectStates& objects);
    bool started(size_t i) const { return started_[i] != 0; }

private:
    std::vector<uint8_t> started_; // per Area::startChecks index
};
