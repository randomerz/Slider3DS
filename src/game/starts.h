// Unity's Start(): it runs once per component, the first time the component's object is active
// (and the component enabled). The components with a Start the port mirrors:
//   - ConditionChecker with checkOnStart: checks its conditionals;
//   - Collectible with shouldDisableAtStart: hides its object (an event shows it later).
#pragma once

#include <cstdint>
#include <vector>

#include "game/area.h"

class World;

class Starts {
public:
    void reset(const Area& area); // nothing started (a fresh scene load)
    // Runs Start for every component that hasn't started and now is active, in export order
    // (checkers, then collectibles), until none are left (a Start may activate other objects).
    void update(World& world);

    bool checkerStarted(size_t i) const { return checkers_[i] != 0; }
    bool collectibleStarted(size_t i) const { return collectibles_[i] != 0; }

private:
    std::vector<uint8_t> checkers_;     // per Area::conditionCheckers index
    std::vector<uint8_t> collectibles_; // per Area::collectibles index
};
