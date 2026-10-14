// Condition.cs / Conditionals: evaluating exported conditions against the game state, and running
// the calls they lead to. (ConditionChecker's Start is in game/world/starts.)
#pragma once

#include "game/data/area.h"

class World;

// Condition.CheckCondition. Unsupported spec conditions are false; spec conditions are the area's
// (village::checkSpec).
bool checkCondition(const ConditionDef& c, const World& world);

// Whether a condition depends only on GameState and Grid (whose version() counters then tell when
// it may have changed): NPCs skip re-checking while those two are unchanged. A new condition kind
// must say which it is.
bool conditionReadsOnlyStateAndGrid(const ConditionDef& c);

// Conditionals.CheckConditions: all conditions true -> run onSuccess's calls, else onFail's.
// Returns whether they all held.
bool runConditionals(World& world, const ConditionalsDef& c);
