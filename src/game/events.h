// Runs exported UnityEvent calls (CallDef) against the World: the methods the Village needs that
// the port supports so far (see CallDef::Method). Unsupported calls do nothing; the exporter's
// report lists them.
#pragma once

#include <vector>

#include "game/area.h"

class World;

void runCall(World& world, const CallDef& call);
void runCalls(World& world, const std::vector<CallDef>& calls);
