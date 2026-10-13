// Unity's Start(): it runs once per component, the first time the component's object is active
// (and the component enabled). The components with a Start the port mirrors:
//   - ConditionChecker with checkOnStart: checks its conditionals;
//   - Collectible with shouldDisableAtStart: hides its object (an event shows it later).
// (SGrid.Start, which hides collectibles already collected, is World::reloadObjects.)
#pragma once

#include <cstdint>
#include <vector>

#include "game/area.h"

class World;

class Starts {
public:
    void reset(const Area& area); // nothing started (a fresh scene load)
    // Runs Start for every component that hasn't started and now is active, object by object,
    // until none are left (a Start may activate other objects). Does nothing if no object changed
    // since the last call.
    void update(World& world);

    bool started(const Area& area, ComponentRef::Kind kind, int index) const;

private:
    std::vector<uint8_t> started_; // per Area::componentRefs entry
    uint32_t seenVersion_ = 0;
    bool fresh_ = true;            // reset() since the last update
};
