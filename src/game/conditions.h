// Condition.cs / Conditionals: evaluating exported conditions against the game state, and running
// the calls they lead to. (ConditionChecker's Start is in game/starts.)
#pragma once

#include "game/area.h"
#include "game/game_state.h"
#include "game/grid.h"

class World;

// Condition.CheckCondition. PlayerCarryingItem and unsupported (spec) conditions are false until
// items and specs exist.
bool checkCondition(const ConditionDef& c, const GameState& state, const Grid& grid);

// Conditionals.CheckConditions: all conditions true -> run onSuccess's calls, else onFail's.
// Returns whether they all held.
bool runConditionals(World& world, const ConditionalsDef& c);
