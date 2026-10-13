// Condition.cs / Conditionals: evaluating exported conditions against the game state, and running
// the calls they lead to. (ConditionChecker's Start is in game/starts.)
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/area.h"
#include "game/grid.h"

class World;

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

// Conditionals.CheckConditions: all conditions true -> run onSuccess's calls, else onFail's.
// Returns whether they all held.
bool runConditionals(World& world, const ConditionalsDef& c);
