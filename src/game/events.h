// Runs exported UnityEvent calls (CallDef) against the World: the methods the port supports so far
// (SLIDER_CALL_METHODS in area.h). Unsupported calls do nothing; the exporter's report lists them.
#pragma once

#include <vector>

#include "game/area.h"

class World;

void runCall(World& world, const CallDef& call);
void runCalls(World& world, const std::vector<CallDef>& calls);
