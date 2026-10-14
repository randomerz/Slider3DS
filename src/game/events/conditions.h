// Condition.cs / Conditionals: evaluating exported conditions against the game state, and running
// the calls they lead to. (ConditionChecker's Start is in game/world/starts.)
#pragma once

#include "game/data/area.h"
#include "game/world/game_state.h"
#include "game/grid/grid.h"

class World;

// Condition.CheckCondition. PlayerCarryingItem and unsupported spec conditions are false until
// items and those specs exist. A condition kind that reads anything but GameState and Grid must say so in
// conditionReadsOnlyStateAndGrid (area.h): NPCs skip re-checking while those two are unchanged.
bool checkCondition(const ConditionDef& c, const GameState& state, const Grid& grid);

// Conditionals.CheckConditions: all conditions true -> run onSuccess's calls, else onFail's.
// Returns whether they all held.
bool runConditionals(World& world, const ConditionalsDef& c);
